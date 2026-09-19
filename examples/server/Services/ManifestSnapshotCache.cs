using System.Collections.Concurrent;
using System.Runtime.CompilerServices;

using Microsoft.Extensions.Caching.Memory;

namespace Nefarius.Vicius.Example.Server.Services;

/// <summary>
///     Single-flight wrapper around <see cref="IMemoryCache" /> for json+minisig snapshots.
///     Concurrent cache misses for the same key share one in-progress factory invocation
///     and receive the same completed value. Only a successful snapshot is stored; a
///     <c>null</c> or faulted factory is forgotten so a later request can retry.
///     In-flight work is scoped to the cache instance so concurrent hosts cannot share
///     each other's <see cref="Lazy{T}" />.
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

    private readonly record struct InflightKey(IMemoryCache Cache, string Key);

    private sealed class InflightKeyComparer : IEqualityComparer<InflightKey>
    {
        internal static readonly InflightKeyComparer Instance = new();

        public bool Equals(InflightKey x, InflightKey y) =>
            ReferenceEquals(x.Cache, y.Cache) &&
            string.Equals(x.Key, y.Key, StringComparison.Ordinal);

        public int GetHashCode(InflightKey obj) =>
            HashCode.Combine(RuntimeHelpers.GetHashCode(obj.Cache), obj.Key);
    }

    private static class Gate<T> where T : class
    {
        private static readonly ConcurrentDictionary<InflightKey, Lazy<Task<T?>>> Inflight = new(InflightKeyComparer.Instance);

        public static async Task<T?> GetOrCreateAsync(
            IMemoryCache cache,
            string key,
            TimeSpan duration,
            Func<Task<T?>> factory)
        {
            if (cache.TryGetValue(key, out T? cached) && cached is not null)
                return cached;

            InflightKey inflightKey = new(cache, key);
            Lazy<Task<T?>> lazy = Inflight.GetOrAdd(
                inflightKey,
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
                Inflight.TryRemove(new KeyValuePair<InflightKey, Lazy<Task<T?>>>(inflightKey, lazy));
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
