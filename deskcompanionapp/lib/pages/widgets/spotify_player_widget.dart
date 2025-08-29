// spotify_player_widget.dart
import 'package:deskcompanionapp/pages/helper/spotify_web_service.dart';
import 'package:flutter/material.dart';

class SpotifyPlayerWidget extends StatefulWidget {
  @override
  _SpotifyPlayerWidgetState createState() => _SpotifyPlayerWidgetState();
}

class _SpotifyPlayerWidgetState extends State<SpotifyPlayerWidget> {
  final SpotifyService _spotifyService = SpotifyService();
  Map<String, dynamic>? _currentPlayback;
  bool _isLoading = false;
  double _volume = 50.0;

  @override
  void initState() {
    super.initState();
    _refreshPlaybackState();
  }

  Future<void> _authenticate() async {
    setState(() => _isLoading = true);
    try {
      await _spotifyService.authenticate();
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Please complete authentication in browser')),
      );
    } catch (e) {
      _showError('Authentication failed: $e');
    } finally {
      setState(() => _isLoading = false);
    }
  }

  Future<void> _handleAuthCode(String code) async {
    setState(() => _isLoading = true);
    try {
      await _spotifyService.handleAuthorizationResponse(code);
      await _refreshPlaybackState();
    } catch (e) {
      _showError('Failed to complete authentication: $e');
    } finally {
      setState(() => _isLoading = false);
    }
  }

  Future<void> _refreshPlaybackState() async {
    if (!_spotifyService.isAuthenticated) return;

    try {
      final playback = await _spotifyService.getCurrentPlayback();
      setState(() => _currentPlayback = playback);
    } catch (e) {
      _showError('Failed to get playback state: $e');
    }
  }

  Future<void> _play() async {
    setState(() => _isLoading = true);
    try {
      await _spotifyService.play();
      await _refreshPlaybackState();
    } catch (e) {
      _showError('Failed to play: $e');
    } finally {
      setState(() => _isLoading = false);
    }
  }

  Future<void> _pause() async {
    setState(() => _isLoading = true);
    try {
      await _spotifyService.pause();
      await _refreshPlaybackState();
    } catch (e) {
      _showError('Failed to pause: $e');
    } finally {
      setState(() => _isLoading = false);
    }
  }

  Future<void> _skipNext() async {
    setState(() => _isLoading = true);
    try {
      await _spotifyService.skipToNext();
      await Future.delayed(
        Duration(milliseconds: 500),
      ); // Wait for Spotify to update
      await _refreshPlaybackState();
    } catch (e) {
      _showError('Failed to skip to next: $e');
    } finally {
      setState(() => _isLoading = false);
    }
  }

  Future<void> _skipPrevious() async {
    setState(() => _isLoading = true);
    try {
      await _spotifyService.skipToPrevious();
      await Future.delayed(
        Duration(milliseconds: 500),
      ); // Wait for Spotify to update
      await _refreshPlaybackState();
    } catch (e) {
      _showError('Failed to skip to previous: $e');
    } finally {
      setState(() => _isLoading = false);
    }
  }

  Future<void> _setVolume(double volume) async {
    try {
      await _spotifyService.setVolume(volume.round());
      setState(() => _volume = volume);
    } catch (e) {
      _showError('Failed to set volume: $e');
    }
  }

  void _showError(String message) {
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(content: Text(message), backgroundColor: Colors.red),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text('Spotify Controller'),
        actions: [
          IconButton(
            icon: Icon(Icons.refresh),
            onPressed: _refreshPlaybackState,
          ),
        ],
      ),
      body: Padding(
        padding: EdgeInsets.all(16.0),
        child: Column(
          children: [
            // Authentication Section
            if (!_spotifyService.isAuthenticated)
              Card(
                child: Padding(
                  padding: EdgeInsets.all(16.0),
                  child: Column(
                    children: [
                      Text('Not authenticated with Spotify'),
                      SizedBox(height: 10),
                      ElevatedButton(
                        onPressed: _isLoading ? null : _authenticate,
                        child: Text('Connect to Spotify'),
                      ),
                      SizedBox(height: 10),
                      TextField(
                        decoration: InputDecoration(
                          labelText: 'Auth Code (paste from redirect)',
                          border: OutlineInputBorder(),
                        ),
                        onSubmitted: _handleAuthCode,
                      ),
                    ],
                  ),
                ),
              ),

            // Current Track Info
            if (_currentPlayback != null)
              Card(
                child: Padding(
                  padding: EdgeInsets.all(16.0),
                  child: Column(
                    children: [
                      if (_currentPlayback!['item'] != null) ...[
                        Text(
                          _currentPlayback!['item']['name'],
                          style: Theme.of(context).textTheme.headlineSmall,
                          textAlign: TextAlign.center,
                        ),
                        Text(
                          _currentPlayback!['item']['artists']
                              .map((a) => a['name'])
                              .join(', '),
                          style: Theme.of(context).textTheme.bodyMedium,
                          textAlign: TextAlign.center,
                        ),
                        SizedBox(height: 10),
                        if (_currentPlayback!['item']['album']['images']
                            .isNotEmpty)
                          Image.network(
                            _currentPlayback!['item']['album']['images'][0]['url'],
                            width: 200,
                            height: 200,
                          ),
                      ],
                    ],
                  ),
                ),
              ),

            // Player Controls
            if (_spotifyService.isAuthenticated)
              Card(
                child: Padding(
                  padding: EdgeInsets.all(16.0),
                  child: Column(
                    children: [
                      // Playback Controls
                      Row(
                        mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                        children: [
                          IconButton(
                            iconSize: 48,
                            onPressed: _isLoading ? null : _skipPrevious,
                            icon: Icon(Icons.skip_previous),
                          ),
                          IconButton(
                            iconSize: 64,
                            onPressed:
                                _isLoading
                                    ? null
                                    : () {
                                      if (_currentPlayback?['is_playing'] ==
                                          true) {
                                        _pause();
                                      } else {
                                        _play();
                                      }
                                    },
                            icon: Icon(
                              _currentPlayback?['is_playing'] == true
                                  ? Icons.pause_circle_filled
                                  : Icons.play_circle_filled,
                            ),
                          ),
                          IconButton(
                            iconSize: 48,
                            onPressed: _isLoading ? null : _skipNext,
                            icon: Icon(Icons.skip_next),
                          ),
                        ],
                      ),

                      SizedBox(height: 20),

                      // Volume Control
                      Row(
                        children: [
                          Icon(Icons.volume_down),
                          Expanded(
                            child: Slider(
                              value: _volume,
                              min: 0,
                              max: 100,
                              divisions: 20,
                              label: '${_volume.round()}%',
                              onChanged: _setVolume,
                            ),
                          ),
                          Icon(Icons.volume_up),
                        ],
                      ),

                      // Additional Controls
                      Row(
                        mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                        children: [
                          ElevatedButton(
                            onPressed: () => _spotifyService.shuffle(true),
                            child: Text('Shuffle On'),
                          ),
                          ElevatedButton(
                            onPressed: () => _spotifyService.shuffle(false),
                            child: Text('Shuffle Off'),
                          ),
                        ],
                      ),
                    ],
                  ),
                ),
              ),

            // Loading Indicator
            if (_isLoading)
              Padding(
                padding: EdgeInsets.all(16.0),
                child: CircularProgressIndicator(),
              ),
          ],
        ),
      ),
    );
  }

  @override
  void dispose() {
    _spotifyService.dispose();
    super.dispose();
  }
}
