# NowPlaying Addon

Addon that exposes Windows System Media Transport Controls (SMTC) playback metadata and transport actions to Novadesk.

## Files
- `NowPlaying.sln` - Visual Studio solution
- `NowPlaying/NowPlaying.vcxproj` - Visual Studio project
- `NowPlaying/main.cpp` - addon entry points and exported API
- `NowPlaying/MediaController.*` - SMTC controller and polling logic
- `NowPlaying/ImageUtils.*` - cover art extraction helpers
- `NowPlaying/NowPlaying.rc` / `NowPlaying/resource.h` - version metadata resource
- `NovadeskAPI/novadesk_addon.h` - addon API header
- `NovadeskAddon.props` - common build props
- `addon.json` - addon name and version used for release packaging

## Build
1. Open `NowPlaying.sln` in Visual Studio 2019+.
2. Build `Debug|x64` or `Release|x64`.
3. Output DLL will be under `dist\x64\...`.
4. For `Release`, a ZIP is created in the same output folder named `Name_vXXX.zip` using `addon.json`.

## Build from terminal
```powershell
.\Build.ps1
.\Build.ps1 -Configuration Release
```

## Usage
```javascript
import { addon } from "novadesk";

const nowPlaying = addon.load("path/to/NowPlaying.dll");

const stats = nowPlaying.stats();
console.log(stats);
```

## API
### `stats([options])`
Returns the current playback stats.

Optional `options`:
- `includeThumbnail` (`boolean`, default `true`): Include `thumbnail` path.
- `includeGenres` (`boolean`, default `true`): Include `genres` string.

Return shape:
- `available` (`boolean`)
- `player` (`string`)
- `artist` (`string`)
- `album` (`string`)
- `title` (`string`)
- `thumbnail` (`string`, optional)
- `duration` (`number`, seconds)
- `position` (`number`, seconds)
- `progress` (`number`, 0..100)
- `state` (`number`, 0=Stopped, 1=Playing, 2=Paused)
- `status` (`number`, 0=Closed, 1=Opened)
- `shuffle` (`boolean`)
- `repeat` (`boolean`)
- `genres` (`string`, optional)

### `backend()`
Returns the backend id string (`"smtc_v2"`).

### `play()` / `pause()` / `playPause()` / `stop()` / `next()` / `previous()`
Dispatch playback transport actions to the active session.

### `setPosition(value, isPercent)`
Seek to a specific position.
- `value` (`number`): seconds or percent (0..100)
- `isPercent` (`boolean`): when true, treat `value` as percent

### `setShuffle(enabled)`
Enable or disable shuffle.

### `setRepeat(mode)`
Set repeat mode (implementation-specific integer).
