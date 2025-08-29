import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_dotenv/flutter_dotenv.dart';
import 'package:spotify_sdk/models/player_state.dart';
import 'package:spotify_sdk/models/track.dart';
import 'package:spotify_sdk/spotify_sdk.dart';

/// A [StatefulWidget] which uses:
/// * [spotify_sdk](https://pub.dev/packages/spotify_sdk)
/// to connect to Spotify and use controls.
///
/// CONTROLS:
/// pause
/// resume
/// seekTo (mili)
/// seekToRelative (skip milis)
/// skipToNext
/// skipToPrev
/// volume

class MusicControls extends StatefulWidget {
  const MusicControls({super.key});

  @override
  State<MusicControls> createState() => _MusicControlsState();
}

class _MusicControlsState extends State<MusicControls> {
  PlayerState? _playerState;
  StreamSubscription<PlayerState>? _playerStateSubscription;

  Future<void> connectToSpotifyRemote() async {
    try {
      var result = await SpotifySdk.connectToSpotifyRemote(
        clientId: dotenv.env['CLIENT_ID'].toString(),
        redirectUrl: dotenv.env['REDIRECT_URL'].toString(),
      );
      print("Connected: $result");

      // Subscribe after connection
      _subscribeToPlayerState();
    } catch (e) {
      print("Connect error: $e");
    }
  }

  void _subscribeToPlayerState() {
    _playerStateSubscription?.cancel();
    _playerStateSubscription = SpotifySdk.subscribePlayerState().listen(
      (playerState) {
        setState(() {
          _playerState = playerState;
        });
      },
      onError: (err) {
        print("PlayerState error: $err");
      },
    );
  }

  @override
  void initState() {
    super.initState();
    _initSpotify();
  }

  Future<void> _initSpotify() async {
    try {
      // Step 1: Authenticate
      var authenticationToken = await SpotifySdk.getAccessToken(
        clientId: dotenv.env['CLIENT_ID'].toString(),
        redirectUrl: dotenv.env['REDIRECT_URL'].toString(),
        scope:
            "app-remote-control,user-modify-playback-state,user-read-playback-state,user-read-currently-playing",
      );

      print("Auth token: $authenticationToken");

      // Step 2: Connect only AFTER successful authentication
      var result = await SpotifySdk.connectToSpotifyRemote(
        clientId: dotenv.env['CLIENT_ID'].toString(),
        redirectUrl: dotenv.env['REDIRECT_URL'].toString(),
      );

      print("Connected: $result");

      // Step 3: Subscribe
      _subscribeToPlayerState();
    } catch (e) {
      print("Init error: $e");
    }
  }

  @override
  void dispose() {
    _playerStateSubscription?.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    Track? track = _playerState?.track;
    return Scaffold(
      appBar: AppBar(title: const Text("Now Playing")),
      body: Column(
        children: [
          Center(
            child:
                track == null
                    ? const Text("No track playing")
                    : Column(
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: [
                        if (track.imageUri.raw.isNotEmpty)
                          Image.network(
                            "https://i.scdn.co/image/${track.imageUri.raw.split(':').last}",
                            width: 200,
                          ),
                        Text(track.name, style: const TextStyle(fontSize: 20)),
                        Text(
                          track.artist.name!,
                          style: const TextStyle(color: Colors.grey),
                        ),
                      ],
                    ),
          ),
          ElevatedButton(
            onPressed: () {
              if (_playerState?.isPaused == true) {
                SpotifySdk.resume();
              } else {
                SpotifySdk.pause();
              }
            },
            child: const Text("Pause/Resume"),
          ),
          Row(
            children: [
              ElevatedButton(
                onPressed: () {
                  SpotifySdk.skipPrevious();
                },
                child: Text("Prev"),
              ),
              ElevatedButton(
                onPressed: () {
                  SpotifySdk.skipNext();
                },
                child: Text("Next"),
              ),
            ],
          ),
          Row(
            children: [
              ElevatedButton(
                onPressed: () {
                  SpotifySdk.seekToRelativePosition(
                    relativeMilliseconds: -10000,
                  );
                },
                child: Text("Prev 10s"),
              ),
              ElevatedButton(
                onPressed: () {
                  SpotifySdk.seekToRelativePosition(
                    relativeMilliseconds: 10000,
                  );
                },
                child: Text("Next 10s"),
              ),
            ],
          ),
        ],
      ),
    );
  }
}
