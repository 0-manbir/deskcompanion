// === Music Display ===
enum MusicSubstate {
  SEEK,
  VOLUME
};
MusicSubstate selectedMusicSubstate = SEEK;

// === Music Info ===
String currentSong = "No Music";
String currentAlbum = "";
String currentArtist = "";
bool musicPlaying = false;
int musicVolume = 50;
int songDuration = 1000; // in ms
int playbackPosition = 0;

// tiny volume icons (8×8)
static void drawMuteIcon(int x, int y) { // speaker + X
  // speaker box
  u8g2.drawLine(x+0, y+3, x+3, y+3);
  u8g2.drawLine(x+0, y+4, x+3, y+4);
  u8g2.drawLine(x+3, y+2, x+5, y+2);
  u8g2.drawLine(x+3, y+5, x+5, y+5);
  u8g2.drawLine(x+5, y+2, x+5, y+5);
  // X
  u8g2.drawLine(x+6, y+1, x+11, y+6);
  u8g2.drawLine(x+11, y+1, x+6, y+6);
}
static void drawMaxIcon(int x, int y) { // speaker + waves
  // speaker
  u8g2.drawLine(x+0, y+3, x+3, y+3);
  u8g2.drawLine(x+0, y+4, x+3, y+4);
  u8g2.drawLine(x+3, y+2, x+5, y+2);
  u8g2.drawLine(x+3, y+5, x+5, y+5);
  u8g2.drawLine(x+5, y+2, x+5, y+5);
  // waves
  u8g2.drawLine(x+7, y+2, x+9, y+4);
  u8g2.drawLine(x+9, y+4, x+7, y+6);
  u8g2.drawLine(x+10, y+1, x+12, y+4);
  u8g2.drawLine(x+12, y+4, x+10, y+7);
}

int scrollOffset = 0;
unsigned long lastPlaybackUpdate = 0;

int seekDuration = 5000; // 5 seconds
int seekTimer = 500; // 0.5s
int relativeMillis = 0;
unsigned long lastSeek = millis();

void updateSeek() {
  if (relativeMillis != 0 && millis() - lastSeek >= seekTimer) {
    sendBLECommand("MUSIC_SEEK_RELATIVE:" + String(relativeMillis));
    relativeMillis = 0;
  }
}

// ---- main render ----
void displayMusicFace() {
  unsigned long now = millis();

  if (musicPlaying) {
    playbackPosition += now - lastPlaybackUpdate;
    if (playbackPosition > songDuration)
      playbackPosition = songDuration;
  }
  lastPlaybackUpdate = now;

  u8g2.clearBuffer();

  // === left circle ===
  const int r = 20;
  const int cx = 20, cy = 25;
  u8g2.drawDisc(cx, cy, r);

  u8g2.setDrawColor(0);
  drawRotatingMusicNote(cx - noteW / 2, cy - noteH / 2, musicPlaying);

  // === right text ===
  const int rightX = 50;
  const int rightW = SCREEN_WIDTH - rightX;

  // Enable clipping for the right area only
  u8g2.setClipWindow(rightX, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);

  // Song name with scrolling
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_ncenB10_tf);
  int songW = u8g2.getUTF8Width(currentSong.c_str());
  if (songW <= rightW) {
    u8g2.drawUTF8(rightX, 18, currentSong.c_str());
  } else {
    scrollOffset = (scrollOffset + 1) % (songW + 20);
    int x = rightX - scrollOffset;
    u8g2.drawUTF8(x, 18, currentSong.c_str());
    u8g2.drawUTF8(x + songW + 20, 18, currentSong.c_str());
  }
  
  // Disable clipping for the rest
  u8g2.setMaxClipWindow();

  // Album + artist with ellipses if too long
  u8g2.setFont(u8g2_font_6x12_tf);

  String albumStr = currentAlbum;
  int albumW = u8g2.getUTF8Width(albumStr.c_str());
  if (albumW > rightW) {
    // shrink until it fits with ".."
    while (albumStr.length() > 0 && u8g2.getUTF8Width((albumStr + "..").c_str()) > rightW) {
      albumStr.remove(albumStr.length() - 1);
    }
    albumStr += "..";
  }
  u8g2.drawUTF8(rightX, 30, albumStr.c_str());

  String artistStr = currentArtist;
  int artistW = u8g2.getUTF8Width(artistStr.c_str());
  if (artistW > rightW) {
    while (artistStr.length() > 0 && u8g2.getUTF8Width((artistStr + "..").c_str()) > rightW) {
      artistStr.remove(artistStr.length() - 1);
    }
    artistStr += "..";
  }
  u8g2.drawUTF8(rightX, 42, artistStr.c_str());

  if (selectedMusicSubstate == SEEK) {
    // === seek bar ===
    u8g2.drawFrame(17, 56, 94, 8); // width of the seek bar: 94px
    int seekBarWidth = map(playbackPosition, 0, songDuration, 0, 94);
    u8g2.drawBox(17, 56, seekBarWidth, 8);
    
    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.drawUTF8(0, SCREEN_HEIGHT, formatTime(playbackPosition/1000).c_str());
    u8g2.drawUTF8(111, SCREEN_HEIGHT, formatTime(songDuration/1000).c_str());
  }
  else if (selectedMusicSubstate == VOLUME) {
    // === volume bar ===
    u8g2.drawFrame(17, 56, 94, 8); // width of the volume bar: 94px
    int volWidth = map(musicVolume, 0, 100, 0, 94);
    u8g2.drawBox(17, 56, volWidth, 8);

    drawMuteIcon(0, 56);
    drawMaxIcon(116, 56);
  }

  u8g2.sendBuffer();
}