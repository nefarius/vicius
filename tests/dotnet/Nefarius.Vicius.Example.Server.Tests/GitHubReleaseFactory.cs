using Octokit;
using Octokit.Internal;

namespace Nefarius.Vicius.Example.Server.Tests;

internal static class GitHubReleaseFactory
{
    private static readonly SimpleJsonSerializer Serializer = new();

    public static Release Create(
        string tagName,
        string name,
        string body,
        params (string FileName, string DownloadUrl, int Size)[] assets)
    {
        string assetJson = string.Join(",", assets.Select((asset, index) =>
            $$"""
              {
                "id": {{index + 1}},
                "name": "{{asset.FileName}}",
                "label": "",
                "content_type": "application/octet-stream",
                "state": "uploaded",
                "size": {{asset.Size}},
                "download_count": 0,
                "created_at": "2024-06-01T12:00:00Z",
                "updated_at": "2024-06-01T12:00:00Z",
                "browser_download_url": "{{asset.DownloadUrl}}",
                "url": "https://api.github.example/assets/{{index + 1}}"
              }
              """));

        string json = $$"""
                        {
                          "url": "https://api.github.example/releases/1",
                          "html_url": "https://github.example/releases/{{tagName}}",
                          "assets_url": "https://api.github.example/releases/1/assets",
                          "upload_url": "https://uploads.github.example/releases/1/assets",
                          "id": 1,
                          "node_id": "R_1",
                          "tag_name": "{{tagName}}",
                          "target_commitish": "master",
                          "name": "{{name}}",
                          "draft": false,
                          "prerelease": false,
                          "created_at": "2024-06-01T12:00:00Z",
                          "published_at": "2024-06-01T12:00:00Z",
                          "body": {{System.Text.Json.JsonSerializer.Serialize(body)}},
                          "tarball_url": "https://api.github.example/tarball",
                          "zipball_url": "https://api.github.example/zipball",
                          "assets": [{{assetJson}}]
                        }
                        """;

        return Serializer.Deserialize<Release>(json);
    }
}
