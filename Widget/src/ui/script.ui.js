console.log("Script loaded");
ipcRenderer.send("nowPlaying_request");

let mediaStats = null;
let bg_width = 300;
let bg_height = 400;
let fontColor = "rgb(255, 255, 255)";
let subFontColor = "rgb(150, 150, 150)";
let buttonSpacing = 30;

//  =================================================================
//  Helper Functions
//  =================================================================

function getElementProperty(id, property) {
  result = ui.getElementProperty(id, property);
  return result;
}

function setElementProperty(id, property, value) {
  ui.setElementProperties(id, { [property]: value });
}

function formatTime(totalSeconds) {
  if (typeof totalSeconds !== "number" || !isFinite(totalSeconds))
    return "00:00";
  const s = Math.max(0, Math.floor(totalSeconds));
  const m = Math.floor(s / 60);
  const ss = s % 60;
  return String(m).padStart(2, "0") + ":" + String(ss).padStart(2, "0");
}

ipcRenderer.on("update-nowPlaying", (event, payload) => {
  mediaStats = payload;

  if (!mediaStats || !mediaStats.thumbnail) {
    setElementProperty("nowPlaying_Image", "path", "default.png");
  } else {
    setElementProperty("nowPlaying_Image", "path", mediaStats.thumbnail);
  }

  setElementProperty("title_Text", "text", mediaStats?.title);
  setElementProperty("artist_Text", "text", mediaStats?.artist);

  setElementProperty("position_Text", "text", formatTime(mediaStats?.position));
  setElementProperty("duration_Text", "text", formatTime(mediaStats?.duration));

  if (typeof mediaStats?.progress === "number" && isFinite(mediaStats.progress)) {
    setElementProperty("nowPlaying_Bar", "value", Math.max(0, Math.min(1, mediaStats.progress / 100)));
  } else if (typeof mediaStats?.duration === "number" && mediaStats.duration > 0 && typeof mediaStats?.position === "number") {
    setElementProperty("nowPlaying_Bar", "value", Math.max(0, Math.min(1, mediaStats.position / mediaStats.duration)));
  }

  if (mediaStats?.state === 1) {
    setElementProperty("play_Text", "text", "||");
  } else {
    setElementProperty("play_Text", "text", ">");
  }

  console.log("NowPlaying:", JSON.stringify(mediaStats));
});

ui.addImage({
  id: "nowPlaying_Image",
  x: (bg_width - 180) / 2,
  y: 10,
  width: 180,
  height: 150,
  path: "default.png",
});

ui.addText({
  id: "title_Text",
  x: bg_width / 2,
  y: 180,
  text: "---",
  fontSize: 16,
  width: bg_width,
  textClip: "ellipsis",
  textAlign: "centercenter",
  fontColor: fontColor,
});

ui.addText({
  id: "artist_Text",
  x: bg_width / 2,
  y: 200,
  text: "---",
  fontSize: 16,
  textClip: "ellipsis",
  width: bg_width,
  textAlign: "centercenter",
  fontColor: subFontColor,
});

ui.addText({
  id: "previous_Text",
  x: (bg_width / 2) - buttonSpacing,
  y: 250,
  text: "<<",
  fontSize: 20,
  textClip: "ellipsis",
  width: bg_width,
  textAlign: "centercenter",
  fontColor: subFontColor,
  onLeftMouseUp: function () {
    ipcRenderer.send("nowPlaying_action", { action: "previous" });
  }
});

ui.addText({
  id: "play_Text",
  x: bg_width / 2,
  y: 250,
  text: ">",
  fontSize: 20,
  textClip: "ellipsis",
  width: bg_width,
  textAlign: "centercenter",
  fontColor: subFontColor,
  onLeftMouseUp: function () {
    ipcRenderer.send("nowPlaying_action", { action: "playPause" });
  }
});

ui.addText({
  id: "next_Text",
  x: (bg_width / 2) + buttonSpacing,
  y: 250,
  text: ">>",
  fontSize: 20,
  textClip: "ellipsis",
  width: bg_width,
  textAlign: "centercenter",
  fontColor: subFontColor,
  onLeftMouseUp: function () {
    ipcRenderer.send("nowPlaying_action", { action: "next" });
  }
});

ui.addText({
  id: "position_Text",
  x: 20,
  y: 280,
  text: "00:00",
  fontSize: 16,
  fontColor: fontColor,
});

ui.addText({
  id: "duration_Text",
  x: bg_width - 20,
  y: 280,
  text: "00:00",
  fontSize: 16,
  textAlign: "right",
  fontColor: fontColor,
});

ui.addBar({
  id: "nowPlaying_Bar",
  x: 10,
  y: 320,
  width: bg_width - 20,
  height: 10,
  value: 1,
  barColor: "rgba(20, 232, 115, 1)",
  backgroundColor: "rgb(200,200,200)",
  onLeftMouseUp: function (e) {
    const pct = Math.max(0, Math.min(100, e?.__offsetXPercent ?? 0));
    ipcRenderer.send("nowPlaying_seek", { percent: pct });
  }
});
