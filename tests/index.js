import { app, addon, tray } from "novadesk";
const tray = new tray();
console.log("=== NowPlayingMonitor Integration (Latest API) ===");

var np = addon.load("../dist/x64/Debug/NowPlaying/NowPlaying.dll");
var nowPlaying = np.nowPlaying;

var id = setInterval(function () {
  console.log("NowPlaying:", JSON.stringify(nowPlaying.stats()));
}, 1000);

// setTimeout(function () {
//   console.log("Unloading NowPlaying");
//   addon.unload(np);
// }, 5000);


tray.setContextMenu([
  {
    text: "Exit",
    action: function () {
      tray.destroy();
      app.exit();
    }
  }
]);

// setTimeout(function () {
//   console.log("Set nowPlaying position to 50%");
//   nowPlaying.setPosition(50, true);
// }, 5000);

// setTimeout(function () {
//   clearInterval(id);
//   console.log("Unloading NowPlaying");
//   addon.unload(np);
//   console.log("Exiting app");
//   app.exit();
// }, 12000);
