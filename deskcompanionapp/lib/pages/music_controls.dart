import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../services/desk_companion_service.dart';

class MusicControls extends StatelessWidget {
  const MusicControls({super.key});

  @override
  Widget build(BuildContext context) {
    return Consumer<DeskCompanionService>(
      builder: (context, service, child) {
        final playerState = service.currentPlayerState;
        final track = playerState?.track;

        return Scaffold(
          body: Column(
            children: [
              // Album Art and Song Info
              Expanded(
                flex: 2,
                child: Container(
                  width: double.infinity,
                  child: Column(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: [
                      // Album Art
                      Container(
                        width: 250,
                        height: 250,
                        decoration: BoxDecoration(
                          borderRadius: BorderRadius.circular(20),
                          boxShadow: [
                            BoxShadow(
                              color: Colors.black26,
                              blurRadius: 20,
                              offset: Offset(0, 10),
                            ),
                          ],
                        ),
                        child:
                            track?.imageUri.raw.isNotEmpty == true
                                ? ClipRRect(
                                  borderRadius: BorderRadius.circular(20),
                                  child: Image.network(
                                    "https://i.scdn.co/image/${track!.imageUri.raw.split(':').last}",
                                    fit: BoxFit.cover,
                                    errorBuilder:
                                        (context, error, stackTrace) =>
                                            _buildPlaceholderArt(),
                                  ),
                                )
                                : _buildPlaceholderArt(),
                      ),

                      SizedBox(height: 30),

                      // Song Title
                      Text(
                        track?.name ?? "No song playing",
                        style: TextStyle(
                          fontSize: 24,
                          fontWeight: FontWeight.bold,
                        ),
                        textAlign: TextAlign.center,
                        maxLines: 2,
                        overflow: TextOverflow.ellipsis,
                      ),

                      SizedBox(height: 8),

                      // Artist Name
                      Text(
                        track?.artist.name ?? "Connect to Spotify",
                        style: TextStyle(fontSize: 18, color: Colors.grey[600]),
                        textAlign: TextAlign.center,
                      ),
                    ],
                  ),
                ),
              ),

              // Progress Bar (placeholder - Spotify SDK doesn't always provide progress)
              Padding(
                padding: EdgeInsets.symmetric(horizontal: 32),
                child: Column(
                  children: [
                    SliderTheme(
                      data: SliderTheme.of(context).copyWith(
                        trackHeight: 4,
                        thumbShape: RoundSliderThumbShape(
                          enabledThumbRadius: 8,
                        ),
                      ),
                      child: Slider(
                        value: 0.3, // Placeholder value
                        onChanged: (value) {
                          // TODO: Implement seek functionality
                        },
                      ),
                    ),
                    Padding(
                      padding: EdgeInsets.symmetric(horizontal: 16),
                      child: Row(
                        mainAxisAlignment: MainAxisAlignment.spaceBetween,
                        children: [
                          Text(
                            "1:23",
                            style: TextStyle(color: Colors.grey[600]),
                          ),
                          Text(
                            "3:45",
                            style: TextStyle(color: Colors.grey[600]),
                          ),
                        ],
                      ),
                    ),
                  ],
                ),
              ),

              SizedBox(height: 20),

              // Control Buttons
              Padding(
                padding: EdgeInsets.symmetric(horizontal: 32),
                child: Row(
                  mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                  children: [
                    _ControlButton(
                      icon: Icons.skip_previous,
                      onPressed: () => service.skipPrevious(),
                      size: 32,
                    ),
                    _ControlButton(
                      icon:
                          playerState?.isPaused == false
                              ? Icons.pause_circle_filled
                              : Icons.play_circle_filled,
                      onPressed: () => service.playPause(),
                      size: 64,
                      isPrimary: true,
                    ),
                    _ControlButton(
                      icon: Icons.skip_next,
                      onPressed: () => service.skipNext(),
                      size: 32,
                    ),
                  ],
                ),
              ),

              SizedBox(height: 30),

              // Additional Controls
              Padding(
                padding: EdgeInsets.symmetric(horizontal: 32),
                child: Row(
                  mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                  children: [
                    _ControlButton(
                      icon: Icons.shuffle,
                      onPressed: () {
                        // TODO: Toggle shuffle
                      },
                      size: 24,
                    ),
                    _ControlButton(
                      icon: Icons.favorite_border,
                      onPressed: () {
                        // TODO: Like/unlike song
                      },
                      size: 24,
                    ),
                    _ControlButton(
                      icon: Icons.repeat,
                      onPressed: () {
                        // TODO: Toggle repeat
                      },
                      size: 24,
                    ),
                  ],
                ),
              ),

              SizedBox(height: 20),

              // Connection Status
              Container(
                padding: EdgeInsets.all(16),
                child: Row(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    Icon(
                      service.isConnected
                          ? Icons.bluetooth_connected
                          : Icons.bluetooth_disabled,
                      size: 16,
                      color: service.isConnected ? Colors.green : Colors.red,
                    ),
                    SizedBox(width: 8),
                    Text(
                      service.isConnected
                          ? "Synced to Desk Companion"
                          : "Not connected",
                      style: TextStyle(color: Colors.grey[600], fontSize: 12),
                    ),
                  ],
                ),
              ),
            ],
          ),
        );
      },
    );
  }

  Widget _buildPlaceholderArt() {
    return Container(
      decoration: BoxDecoration(
        borderRadius: BorderRadius.circular(20),
        gradient: LinearGradient(
          begin: Alignment.topLeft,
          end: Alignment.bottomRight,
          colors: [Colors.purple.shade300, Colors.blue.shade300],
        ),
      ),
      child: Center(
        child: Icon(Icons.music_note, size: 80, color: Colors.white),
      ),
    );
  }
}

class _ControlButton extends StatelessWidget {
  final IconData icon;
  final VoidCallback onPressed;
  final double size;
  final bool isPrimary;

  const _ControlButton({
    required this.icon,
    required this.onPressed,
    this.size = 32,
    this.isPrimary = false,
  });

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTap: onPressed,
      child: Container(
        padding: EdgeInsets.all(isPrimary ? 16 : 12),
        decoration: BoxDecoration(
          shape: BoxShape.circle,
          color:
              isPrimary ? Theme.of(context).primaryColor : Colors.grey.shade200,
          boxShadow:
              isPrimary
                  ? [
                    BoxShadow(
                      color: Theme.of(context).primaryColor.withOpacity(0.3),
                      blurRadius: 15,
                      offset: Offset(0, 5),
                    ),
                  ]
                  : null,
        ),
        child: Icon(
          icon,
          size: size,
          color: isPrimary ? Colors.white : Colors.grey.shade700,
        ),
      ),
    );
  }
}
