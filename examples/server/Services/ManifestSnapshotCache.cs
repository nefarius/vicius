using System.Collections.Concurrent;

using Microsoft.Extensions.Caching.Memory;

namespace Nefarius.Vicius.Example.Server.Services;

/// <summary>
///     Single-flight wrapper around <see cref="IMemoryCache" /> for json+minisig snapshots.
///     Concurrent cache misses for the same key share one in-progress factory invocation
///     and receive the same completed value. Only a successful snapshot is stored; a
///     <c>null</c> or faulted factory is forgotten so a later request can retry.
/// </summary>
internal static class ManifestSnapshotCache
{
    public static Task<T?> GetOrCreateAsync<T>(
        IMemoryCache cache,
        string key,
        TimeSpan duration,
        Func<Task<T?>> factory)
        where T : class =>
        Gate<T>.GetOrCreateAsync(cache, key, duration, factory);

    private static class Gate<T> where T : class
    {
        private static readonly ConcurrentDictionary<string, Lazy<Task<T?>>> Inflight = new();

        public static async Task<T?> GetOrCreateAsync(
            IMemoryCache cache,
            string key,
            TimeSpan duration,
            Func<Task<T?>> factory)
        {
            if (cache.TryGetValue(key, out T? cached) && cached is not null)
                return cached;

            Lazy<Task<T?>> lazy = Inflight.GetOrAdd(
                key,
                _ => new Lazy<Task<T?>>(
                    () => CreateAsync(cache, key, duration, factory),
                    LazyThreadSafetyMode.ExecutionAndPublication));

            try
            {
                return await lazy.Value.ConfigureAwait(false);
            }
            finally
            {
                // Drop this in-flight slot so a later miss (or a failed attempt) can retry.
                // Compare-remove so a newer Lazy for the same key is not discarded.
                Inflight.TryRemove(new KeyValuePair<string, Lazy<Task<T?>>>(key, lazy));
            }
        }

        private static async Task<T?> CreateAsync(
            IMemoryCache cache,
            string key,
            TimeSpan duration,
            Func<Task<T?>> factory)
        {
            // Another waiter may have completed and populated the cache between
            // the outer TryGetValue miss and GetOrAdd; do not rebuild in that case.
            if (cache.TryGetValue(key, out T? existing) && existing is not null)
                return existing;

            T? value = await factory().ConfigureAwait(false);
            if (value is not null)
            {
                cache.Set(key, value, new MemoryCacheEntryOptions
                {
                    AbsoluteExpirationRelativeToNow = duration
                });
            }

            return value;
        }
    }
}
