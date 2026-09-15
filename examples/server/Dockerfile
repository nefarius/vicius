FROM mcr.microsoft.com/dotnet/aspnet:10.0 AS base
WORKDIR /app
EXPOSE 80
EXPOSE 443

FROM mcr.microsoft.com/dotnet/sdk:10.0 AS build
WORKDIR /src
COPY ["examples/server/server.csproj", "examples/server/"]
COPY ["abstractions/src/Nefarius.Vicius.Abstractions.csproj", "abstractions/src/"]
COPY ["abstractions/Directory.Build.props", "abstractions/"]
COPY ["abstractions/Directory.Build.targets", "abstractions/"]
RUN dotnet restore "examples/server/server.csproj"
COPY examples/server/ examples/server/
COPY abstractions/src/ abstractions/src/
COPY abstractions/README.md abstractions/README.md
COPY assets/NSS-128x128.png assets/NSS-128x128.png
WORKDIR "/src/examples/server"
RUN dotnet build "server.csproj" -c Release -o /app/build

FROM build AS publish
RUN dotnet publish "server.csproj" -c Release -o /app/publish /p:UseAppHost=false

FROM base AS final
WORKDIR /app
COPY --from=publish /app/publish .
USER $APP_UID
ENTRYPOINT ["dotnet", "server.dll"]
