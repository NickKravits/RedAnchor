// redanchor-steam — библиотека и профиль Steam через SteamKit2 для оболочки RedAnchor.
//
//   redanchor-steam library --user <аккаунт>              (токен входа — первой строкой на stdin)
//   redanchor-steam launch  --user <аккаунт> --app <id>   (то же)
//
// Входит тем же токеном, который сохранил DepotDownloader после входа (по QR-коду или
// паролю), и печатает JSON.
//   library: { "persona": "Имя в профиле", "avatar": "https://…_full.jpg", "steamid": "…",
//              "games": [ { "appid": 220, "name": "Half-Life 2" }, … ] }
//            Игры — все лицензии аккаунта (пакеты → приложения), только тип «game».
//   launch:  { "executable": "bin/game.exe", "arguments": "…", "workingdir": "…" } —
//            как запускать версию игры для Windows (из данных приложения Steam).
// Коды выхода: 0 — успех, 1 — неверные аргументы, 2 — нет связи / таймаут,
//              3 — вход недействителен (нужно войти заново).

using System.Text.Json;
using SteamKit2;

var command = args.Length > 0 ? args[0] : "";
var user = args.Length > 2 && args[1] == "--user" ? args[2] : "";
uint launchApp = 0;
if (command == "launch" && (args.Length < 5 || args[3] != "--app" || !uint.TryParse(args[4], out launchApp)))
    command = "";
if ((command != "library" && command != "launch") || user.Length == 0)
{
    Console.Error.WriteLine("usage: redanchor-steam library --user <account>\n" +
                            "       redanchor-steam launch --user <account> --app <id>   (refresh token on stdin)");
    return 1;
}
var token = Console.In.ReadLine()?.Trim();
if (string.IsNullOrEmpty(token))
{
    Console.Error.WriteLine("no token on stdin");
    return 1;
}

var client = new SteamClient();
var manager = new CallbackManager(client);
var steamUser = client.GetHandler<SteamUser>()!;
var steamApps = client.GetHandler<SteamApps>()!;
var steamFriends = client.GetHandler<SteamFriends>()!;

var loggedOn = new TaskCompletionSource<EResult>(TaskCreationOptions.RunContinuationsAsynchronously);
var licenses = new TaskCompletionSource<IReadOnlyCollection<SteamApps.LicenseListCallback.License>>(TaskCreationOptions.RunContinuationsAsynchronously);
var persona = new TaskCompletionSource<string>(TaskCreationOptions.RunContinuationsAsynchronously);
var avatar = new TaskCompletionSource<string>(TaskCreationOptions.RunContinuationsAsynchronously);
var running = true;

manager.Subscribe<SteamClient.ConnectedCallback>(_ =>
    steamUser.LogOn(new SteamUser.LogOnDetails { Username = user, AccessToken = token, ShouldRememberPassword = true }));
manager.Subscribe<SteamClient.DisconnectedCallback>(_ =>
{
    loggedOn.TrySetResult(EResult.NoConnection);
    licenses.TrySetException(new TimeoutException("disconnected"));
    running = false;
});
manager.Subscribe<SteamUser.LoggedOnCallback>(cb => loggedOn.TrySetResult(cb.Result));
manager.Subscribe<SteamApps.LicenseListCallback>(cb => licenses.TrySetResult(cb.LicenseList));
manager.Subscribe<SteamUser.AccountInfoCallback>(cb => persona.TrySetResult(cb.PersonaName));
manager.Subscribe<SteamFriends.PersonaStateCallback>(cb =>
{
    if (cb.FriendID != steamUser.SteamID)
        return;
    if (!string.IsNullOrEmpty(cb.Name))
        persona.TrySetResult(cb.Name);
    if (cb.AvatarHash is { Length: > 0 } hash && hash.Any(b => b != 0))
        avatar.TrySetResult($"https://avatars.steamstatic.com/{Convert.ToHexString(hash).ToLowerInvariant()}_full.jpg");
});

// Обработка ответов Steam — в отдельном потоке
var pump = Task.Run(() =>
{
    while (running)
        manager.RunWaitCallbacks(TimeSpan.FromMilliseconds(200));
});
client.Connect();

static async Task<T?> Within<T>(Task<T> task, int milliseconds) where T : class =>
    await Task.WhenAny(task, Task.Delay(milliseconds)) == task ? task.Result : null;

try
{
    var result = await loggedOn.Task.WaitAsync(TimeSpan.FromSeconds(30));
    if (result != EResult.OK)
    {
        Console.Error.WriteLine($"logon failed: {result}");
        return result is EResult.InvalidPassword or EResult.AccessDenied or EResult.Expired
                      or EResult.InvalidSignature or EResult.Revoked ? 3 : 2;
    }
    if (command == "launch")
    {
        // Параметры запуска версии для Windows из данных приложения (config → launch)
        var tokens = await steamApps.PICSGetAccessTokens(new[] { launchApp }, Array.Empty<uint>());
        var request = new SteamApps.PICSRequest(launchApp, tokens.AppTokens.TryGetValue(launchApp, out var appToken) ? appToken : 0);
        var products = await steamApps.PICSGetProductInfo(new[] { request }, Array.Empty<SteamApps.PICSRequest>());
        var app = (products.Results ?? Enumerable.Empty<SteamApps.PICSProductInfoCallback>())
            .SelectMany(r => r.Apps).Where(a => a.Key == launchApp).Select(a => a.Value).FirstOrDefault();
        if (app is null)
        {
            Console.Error.WriteLine($"no app info for {launchApp}");
            return 2;
        }
        KeyValue? chosen = null;
        foreach (var entry in app.KeyValues["config"]["launch"].Children)
        {
            var oslist = (entry["config"]["oslist"].Value ?? "").ToLowerInvariant();
            var type = (entry["type"].Value ?? "default").ToLowerInvariant();
            if (oslist.Length > 0 && !oslist.Contains("windows"))
                continue;
            if (type is not ("default" or "none" or ""))
                continue;
            chosen = entry;
            break;
        }
        if (chosen is null || string.IsNullOrEmpty(chosen["executable"].Value))
        {
            Console.Error.WriteLine($"no windows launch entry for {launchApp}");
            return 2;
        }
        Console.WriteLine(JsonSerializer.Serialize(new
        {
            executable = chosen["executable"].Value!.Replace('\\', '/'),
            arguments = chosen["arguments"].Value ?? "",
            workingdir = (chosen["workingdir"].Value ?? "").Replace('\\', '/'),
        }));
        return 0;
    }

    steamFriends.RequestFriendInfo(new[] { steamUser.SteamID! },
                                   EClientPersonaStateFlag.PlayerName | EClientPersonaStateFlag.Presence);

    // Лицензии → пакеты → приложения
    var licenseList = await licenses.Task.WaitAsync(TimeSpan.FromSeconds(30));
    var appIds = new HashSet<uint>();
    foreach (var chunk in licenseList.Select(l => new SteamApps.PICSRequest(l.PackageID, l.AccessToken)).Chunk(200))
    {
        var packages = await steamApps.PICSGetProductInfo(Array.Empty<SteamApps.PICSRequest>(), chunk);
        foreach (var info in packages.Results ?? Enumerable.Empty<SteamApps.PICSProductInfoCallback>())
            foreach (var package in info.Packages.Values)
                foreach (var app in package.KeyValues["appids"].Children)
                    appIds.Add(app.AsUnsignedInteger());
    }

    // Названия и типы приложений (для части приложений нужен токен доступа)
    var games = new List<object>();
    foreach (var chunk in appIds.Chunk(200))
    {
        var tokens = await steamApps.PICSGetAccessTokens(chunk, Array.Empty<uint>());
        var requests = chunk.Select(id => new SteamApps.PICSRequest(id, tokens.AppTokens.TryGetValue(id, out var t) ? t : 0));
        var products = await steamApps.PICSGetProductInfo(requests, Array.Empty<SteamApps.PICSRequest>());
        foreach (var info in products.Results ?? Enumerable.Empty<SteamApps.PICSProductInfoCallback>())
        {
            foreach (var (id, app) in info.Apps)
            {
                var common = app.KeyValues["common"];
                var type = (common["type"].Value ?? "").ToLowerInvariant();
                var name = common["name"].Value ?? "";
                if (type == "game" && name.Length > 0)
                    games.Add(new { appid = id, name });
            }
        }
    }

    var personaName = await Within(persona.Task, 3000) ?? "";
    var avatarUrl = await Within(avatar.Task, 5000) ?? "";
    Console.WriteLine(JsonSerializer.Serialize(new
    {
        persona = personaName,
        avatar = avatarUrl,
        steamid = steamUser.SteamID?.ConvertToUInt64().ToString() ?? "",
        games,
    }));
    return 0;
}
catch (Exception e) when (e is TimeoutException or TaskCanceledException)
{
    Console.Error.WriteLine($"timeout: {e.Message}");
    return 2;
}
finally
{
    if (steamUser.SteamID is not null)
        steamUser.LogOff();
    running = false;
    client.Disconnect();
    await Task.WhenAny(pump, Task.Delay(2000));
}
