// ============================================================================
// NowPlaying Widget - UI Script
// ============================================================================
// This script handles the user interface for the NowPlaying widget,
// displaying media information and providing playback controls.
// ============================================================================

console.log("Script loaded");

// Request initial now playing data from the main process
ipcRenderer.send("nowPlaying_request");

// ============================================================================
// Configuration & State Variables
// ============================================================================

/** @type {Object|null} Media statistics object containing track information */
let mediaStats = null;

// Widget Dimensions
const WIDGET_WIDTH = 300;
const WIDGET_HEIGHT = 400;

// Layout Constants
const ALBUM_ART_SIZE = 180;
const ALBUM_ART_Y = 10;
const TITLE_Y = 180;
const ARTIST_Y = 200;
const CONTROLS_Y = 250;
const TIME_LABELS_Y = 280;
const PROGRESS_BAR_Y = 320;

// Spacing & Alignment
const BUTTON_SPACING = 30;
const HORIZONTAL_PADDING = 10;
const VERTICAL_PADDING = 10;

// Colors
const PRIMARY_COLOR = "rgb(255, 255, 255)";
const SECONDARY_COLOR = "rgb(150, 150, 150)";
const PROGRESS_BAR_COLOR = "rgba(20, 232, 115, 1)";
const PROGRESS_BAR_BG_COLOR = "rgb(200,200,200)";

// Typography
const TITLE_FONT_SIZE = 16;
const ARTIST_FONT_SIZE = 16;
const BUTTON_FONT_SIZE = 20;
const TIME_FONT_SIZE = 16;

// Default Values
const DEFAULT_IMAGE_PATH = "default.png";
const DEFAULT_TITLE = "---";
const DEFAULT_ARTIST = "---";
const DEFAULT_TIME = "00:00";

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Gets a property value from a UI element
 * @param {string} id - The ID of the UI element
 * @param {string} property - The property name to retrieve
 * @returns {*} The property value
 */
function getElementProperty(id, property) {
  const result = ui.getElementProperty(id, property);
  return result;
}

/**
 * Sets a property value on a UI element
 * @param {string} id - The ID of the UI element
 * @param {string} property - The property name to set
 * @param {*} value - The value to assign
 */
function setElementProperty(id, property, value) {
  ui.setElementProperties(id, { [property]: value });
}

/**
 * Formats time in seconds to MM:SS format
 * @param {number} totalSeconds - Total seconds to format
 * @returns {string} Formatted time string (MM:SS)
 */
function formatTime(totalSeconds) {
  if (typeof totalSeconds !== "number" || !isFinite(totalSeconds)) {
    return "00:00";
  }

  const s = Math.max(0, Math.floor(totalSeconds));
  const m = Math.floor(s / 60);
  const ss = s % 60;

  return String(m).padStart(2, "0") + ":" + String(ss).padStart(2, "0");
}

// ============================================================================
// IPC Event Listeners
// ============================================================================

/**
 * Listens for now playing updates from the main process
 * Updates all UI elements with current media information
 */
ipcRenderer.on("update-nowPlaying", (event, payload) => {
  mediaStats = payload;

  // Update album artwork
  if (!mediaStats || !mediaStats.thumbnail) {
    setElementProperty("nowPlaying_Image", "path", "default.png");
  } else {
    setElementProperty("nowPlaying_Image", "path", mediaStats.thumbnail);
  }

  // Update track title and artist
  setElementProperty("title_Text", "text", mediaStats?.title);
  setElementProperty("artist_Text", "text", mediaStats?.artist);

  // Update time display
  setElementProperty("position_Text", "text", formatTime(mediaStats?.position));
  setElementProperty("duration_Text", "text", formatTime(mediaStats?.duration));

  // Update progress bar (supports both percentage and ratio-based progress)
  if (
    typeof mediaStats?.progress === "number" &&
    isFinite(mediaStats.progress)
  ) {
    setElementProperty(
      "nowPlaying_Bar",
      "value",
      Math.max(0, Math.min(1, mediaStats.progress / 100))
    );
  } else if (
    typeof mediaStats?.duration === "number" &&
    mediaStats.duration > 0 &&
    typeof mediaStats?.position === "number"
  ) {
    setElementProperty(
      "nowPlaying_Bar",
      "value",
      Math.max(0, Math.min(1, mediaStats.position / mediaStats.duration))
    );
  }

  // Update play/pause button icon based on playback state
  // State 1 = Playing (show pause icon), otherwise show play icon
  if (mediaStats?.state === 1) {
    setElementProperty("play_Text", "text", "||");
  } else {
    setElementProperty("play_Text", "text", ">");
  }

  console.log("NowPlaying:", JSON.stringify(mediaStats));
});

// ============================================================================
// UI Element Creation
// ============================================================================

/**
 * Album Artwork Image
 * Displayed at the top of the widget, centered horizontally
 */
ui.addImage({
  id: "nowPlaying_Image",
  x: (WIDGET_WIDTH - ALBUM_ART_SIZE) / 2,
  y: ALBUM_ART_Y,
  width: ALBUM_ART_SIZE,
  height: ALBUM_ART_SIZE * (150 / 180), // Maintain aspect ratio
  path: DEFAULT_IMAGE_PATH,
});

/**
 * Track Title Text
 * Centered below album artwork, uses primary font color
 */
ui.addText({
  id: "title_Text",
  x: WIDGET_WIDTH / 2,
  y: TITLE_Y,
  text: DEFAULT_TITLE,
  fontSize: TITLE_FONT_SIZE,
  width: WIDGET_WIDTH,
  textClip: "ellipsis",
  textAlign: "centercenter",
  fontColor: PRIMARY_COLOR,
});

/**
 * Artist Name Text
 * Displayed below title, uses secondary font color
 */
ui.addText({
  id: "artist_Text",
  x: WIDGET_WIDTH / 2,
  y: ARTIST_Y,
  text: DEFAULT_ARTIST,
  fontSize: ARTIST_FONT_SIZE,
  textClip: "ellipsis",
  width: WIDGET_WIDTH,
  textAlign: "centercenter",
  fontColor: SECONDARY_COLOR,
});

/**
 * Previous Track Button
 * Left control button in the playback controls row
 */
ui.addText({
  id: "previous_Text",
  x: WIDGET_WIDTH / 2 - BUTTON_SPACING,
  y: CONTROLS_Y,
  text: "<<",
  fontSize: BUTTON_FONT_SIZE,
  textClip: "ellipsis",
  width: WIDGET_WIDTH,
  textAlign: "centercenter",
  fontColor: SECONDARY_COLOR,
  onLeftMouseUp: function () {
    ipcRenderer.send("nowPlaying_action", { action: "previous" });
  },
});

/**
 * Play/Pause Toggle Button
 * Center control button, icon changes based on playback state
 */
ui.addText({
  id: "play_Text",
  x: WIDGET_WIDTH / 2,
  y: CONTROLS_Y,
  text: ">",
  fontSize: BUTTON_FONT_SIZE,
  textClip: "ellipsis",
  width: WIDGET_WIDTH,
  textAlign: "centercenter",
  fontColor: SECONDARY_COLOR,
  onLeftMouseUp: function () {
    ipcRenderer.send("nowPlaying_action", { action: "playPause" });
  },
});

/**
 * Next Track Button
 * Right control button in the playback controls row
 */
ui.addText({
  id: "next_Text",
  x: WIDGET_WIDTH / 2 + BUTTON_SPACING,
  y: CONTROLS_Y,
  text: ">>",
  fontSize: BUTTON_FONT_SIZE,
  textClip: "ellipsis",
  width: WIDGET_WIDTH,
  textAlign: "centercenter",
  fontColor: SECONDARY_COLOR,
  onLeftMouseUp: function () {
    ipcRenderer.send("nowPlaying_action", { action: "next" });
  },
});

/**
 * Current Position Text
 * Displays elapsed time on the left side above progress bar
 */
ui.addText({
  id: "position_Text",
  x: HORIZONTAL_PADDING * 2,
  y: TIME_LABELS_Y,
  text: DEFAULT_TIME,
  fontSize: TIME_FONT_SIZE,
  fontColor: PRIMARY_COLOR,
});

/**
 * Total Duration Text
 * Displays total track duration on the right side above progress bar
 */
ui.addText({
  id: "duration_Text",
  x: WIDGET_WIDTH - HORIZONTAL_PADDING * 2,
  y: TIME_LABELS_Y,
  text: DEFAULT_TIME,
  fontSize: TIME_FONT_SIZE,
  textAlign: "right",
  fontColor: PRIMARY_COLOR,
});

/**
 * Progress Bar
 * Interactive seek bar allowing users to click/drag to change playback position
 * Positioned at the bottom of the widget
 */
ui.addBar({
  id: "nowPlaying_Bar",
  x: HORIZONTAL_PADDING,
  y: PROGRESS_BAR_Y,
  width: WIDGET_WIDTH - (HORIZONTAL_PADDING * 2),
  height: 10,
  value: 1,
  barColor: PROGRESS_BAR_COLOR,
  backgroundColor: PROGRESS_BAR_BG_COLOR,
  onLeftMouseUp: function (e) {
    // Calculate seek position as percentage (0-100) based on click location
    const pct = Math.max(0, Math.min(100, e?.__offsetXPercent ?? 0));
    ipcRenderer.send("nowPlaying_seek", { percent: pct });
  },
});
