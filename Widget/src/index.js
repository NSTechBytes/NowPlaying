import { widgetWindow, addon } from "novadesk";

const nowPlaying_Addon = addon.load(
  "D:\\Novadesk-Project\\NowPlaying\\dist\\x64\\Debug\\NowPlaying\\NowPlaying.dll"
);

function pushNowPlaying() {
  ipcMain.send("update-nowPlaying", nowPlaying_Addon.stats());
}

pushNowPlaying();
setInterval(() => {
  pushNowPlaying();
}, 1000);

ipcMain.on("nowPlaying_seek", (event, data) => {
  const pct = data && typeof data.percent === "number" ? data.percent : 0;
  const clamped = Math.max(0, Math.min(100, pct));
  nowPlaying_Addon.setPosition(clamped, true);
});

ipcMain.on("nowPlaying_request", () => {
  pushNowPlaying();
});

ipcMain.on("nowPlaying_action", (event, data) => {
  const action = data && data.action ? data.action : "";
  if (action === "previous") nowPlaying_Addon.previous();
  else if (action === "next") nowPlaying_Addon.next();
  else if (action === "playPause") nowPlaying_Addon.playPause();
});

var nowPlayingWindow = new widgetWindow({
  id: "now-playing",
  width: 300,
  height: 400,
  backgroundColor: "#000000",
  script: "ui/script.ui.js",
});
