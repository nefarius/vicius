# vīcĭus єאค๓קɭє รєгשєг

You found the example backend implementation! 🎉

I also use this project in production so it can be considered stable.

## Run the demo locally

The example server ships with a pre-wired happy-path demo that exercises every
working UI surface of the updater wizard.  After a fresh clone, getting a full
end-to-end run takes two steps:

**1. Start the example server** (Visual Studio: open `examples.sln`, set
`server` as the startup project, press F5 — or from the command line):

```PowerShell
cd examples/server
dotnet run
# Listening on http://localhost:5200
```

**2. Start the updater in Debug** (Visual Studio: open `vīcĭus.sln`, set the
`vīcĭus` project as the startup project, select **Debug | x64**, press F5).

The project ships a committed `src/vīcĭus.vcxproj.user` that passes these
arguments automatically:

```
--server-url http://localhost:5200/api/demo/Showcase/updates.json
--log-level debug
--log-to-file $(TargetDir)debug.log
```

The updater will:
- Detect the installed version as **0.0.1** (always outdated), so the wizard
  always appears.
- Render the rich Markdown changelog (headings, lists, image, scrollbars, link).
- Show the **Help** and **Remind me tomorrow** buttons.
- Download the ~50 MB Microsoft-signed .NET Desktop Runtime (progress bar).
- Verify the Authenticode signature (WhenPresent mode — passes for signed files).
- Accept exit codes **0** and **3010** as success.

A `debug.log` file is written next to `Updater.exe` for each run.

Demo endpoint source: [`Endpoints/DefaultDemoEndpoint.cs`](Endpoints/DefaultDemoEndpoint.cs)

---

## How to build

Tag it with your own registry, of course 😉

```PowerShell
docker build --push -t nefarius.azurecr.io/nefarius-vicius-server:latest .
```

## Sources & 3rd party credits

- [NJsonSchema for .NET](https://github.com/RicoSuter/NJsonSchema)
- [FastEndpoints](https://fast-endpoints.com/)
- [Nefarius.Utilities.AspNetCore](https://github.com/nefarius/Nefarius.Utilities.AspNetCore)
- [Nefarius.Vicius.Abstractions](../../abstractions/) (co-located in this repo)
