// spotify_service.dart
import 'dart:convert';
import 'dart:math';
import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;
import 'package:oauth2/oauth2.dart' as oauth2;
import 'package:url_launcher/url_launcher.dart';
import 'package:flutter_dotenv/flutter_dotenv.dart';

class SpotifyService {
  static const String _authorizationEndpoint =
      'https://accounts.spotify.com/authorize';
  static const String _tokenEndpoint = 'https://accounts.spotify.com/api/token';
  static const String _apiBaseUrl = 'https://api.spotify.com/v1';

  oauth2.Client? _client;

  // Scopes needed for playback control
  static const List<String> _scopes = [
    'user-modify-playback-state',
    'user-read-playback-state',
    'user-read-currently-playing',
    'streaming',
  ];

  bool get isAuthenticated => _client != null;

  // Step 1: Start OAuth flow
  Future<void> authenticate() async {
    final clientId = dotenv.env['CLIENT_ID']!;
    final redirectUrl = Uri.parse(dotenv.env['REDIRECT_URL']!);

    // Generate a random state for security
    final state = _generateRandomString(16);

    final grant = oauth2.AuthorizationCodeGrant(
      clientId,
      Uri.parse(_authorizationEndpoint),
      Uri.parse(_tokenEndpoint),
    );

    // Get the authorization URL
    final authorizationUrl = grant.getAuthorizationUrl(
      redirectUrl,
      scopes: _scopes,
      state: state,
    );

    // Launch the authorization URL
    if (!await launchUrl(
      authorizationUrl,
      mode: LaunchMode.externalApplication,
    )) {
      throw Exception('Could not launch authorization URL');
    }

    // Wait for the redirect (you'll need to handle this in your app)
    // This is a simplified version - you'll need to listen for the redirect
    print('Please complete authorization in browser and return the code');
  }

  // Step 2: Handle the redirect and exchange code for tokens
  Future<void> handleAuthorizationResponse(String authorizationCode) async {
    final clientId = dotenv.env['CLIENT_ID']!;
    final redirectUrl = Uri.parse(dotenv.env['REDIRECT_URL']!);

    final grant = oauth2.AuthorizationCodeGrant(
      clientId,
      Uri.parse(_authorizationEndpoint),
      Uri.parse(_tokenEndpoint),
    );

    try {
      _client = await grant.handleAuthorizationResponse({
        'code': authorizationCode,
        'redirect_uri': redirectUrl.toString(),
      });

      print('Successfully authenticated with Spotify!');
    } catch (e) {
      throw Exception('Failed to authenticate: $e');
    }
  }

  // Player Controls
  Future<void> play({String? contextUri, List<String>? uris}) async {
    _ensureAuthenticated();

    final body = <String, dynamic>{};
    if (contextUri != null) body['context_uri'] = contextUri;
    if (uris != null) body['uris'] = uris;

    final response = await _client!.put(
      Uri.parse('$_apiBaseUrl/me/player/play'),
      headers: {'Content-Type': 'application/json'},
      body: body.isNotEmpty ? jsonEncode(body) : null,
    );

    if (response.statusCode != 204) {
      throw Exception('Failed to play: ${response.body}');
    }
  }

  Future<void> pause() async {
    _ensureAuthenticated();

    final response = await _client!.put(
      Uri.parse('$_apiBaseUrl/me/player/pause'),
    );

    if (response.statusCode != 204) {
      throw Exception('Failed to pause: ${response.body}');
    }
  }

  Future<void> skipToNext() async {
    _ensureAuthenticated();

    final response = await _client!.post(
      Uri.parse('$_apiBaseUrl/me/player/next'),
    );

    if (response.statusCode != 204) {
      throw Exception('Failed to skip to next: ${response.body}');
    }
  }

  Future<void> skipToPrevious() async {
    _ensureAuthenticated();

    final response = await _client!.post(
      Uri.parse('$_apiBaseUrl/me/player/previous'),
    );

    if (response.statusCode != 204) {
      throw Exception('Failed to skip to previous: ${response.body}');
    }
  }

  Future<void> setVolume(int volumePercent) async {
    _ensureAuthenticated();

    if (volumePercent < 0 || volumePercent > 100) {
      throw ArgumentError('Volume must be between 0 and 100');
    }

    final response = await _client!.put(
      Uri.parse('$_apiBaseUrl/me/player/volume?volume_percent=$volumePercent'),
    );

    if (response.statusCode != 204) {
      throw Exception('Failed to set volume: ${response.body}');
    }
  }

  Future<void> shuffle(bool state) async {
    _ensureAuthenticated();

    final response = await _client!.put(
      Uri.parse('$_apiBaseUrl/me/player/shuffle?state=$state'),
    );

    if (response.statusCode != 204) {
      throw Exception('Failed to set shuffle: ${response.body}');
    }
  }

  Future<void> setRepeatMode(String state) async {
    _ensureAuthenticated();

    // state can be 'track', 'context' or 'off'
    final response = await _client!.put(
      Uri.parse('$_apiBaseUrl/me/player/repeat?state=$state'),
    );

    if (response.statusCode != 204) {
      throw Exception('Failed to set repeat: ${response.body}');
    }
  }

  // Get current playback state
  Future<Map<String, dynamic>?> getCurrentPlayback() async {
    _ensureAuthenticated();

    final response = await _client!.get(Uri.parse('$_apiBaseUrl/me/player'));

    if (response.statusCode == 200) {
      return jsonDecode(response.body);
    } else if (response.statusCode == 204) {
      return null; // No active playback
    } else {
      throw Exception('Failed to get playback state: ${response.body}');
    }
  }

  // Get currently playing track
  Future<Map<String, dynamic>?> getCurrentlyPlaying() async {
    _ensureAuthenticated();

    final response = await _client!.get(
      Uri.parse('$_apiBaseUrl/me/player/currently-playing'),
    );

    if (response.statusCode == 200) {
      return jsonDecode(response.body);
    } else if (response.statusCode == 204) {
      return null; // Nothing playing
    } else {
      throw Exception('Failed to get currently playing: ${response.body}');
    }
  }

  // Get user's devices
  Future<List<dynamic>> getDevices() async {
    _ensureAuthenticated();

    final response = await _client!.get(
      Uri.parse('$_apiBaseUrl/me/player/devices'),
    );

    if (response.statusCode == 200) {
      final data = jsonDecode(response.body);
      return data['devices'];
    } else {
      throw Exception('Failed to get devices: ${response.body}');
    }
  }

  // Transfer playback to a device
  Future<void> transferPlayback(String deviceId, {bool play = true}) async {
    _ensureAuthenticated();

    final body = {
      'device_ids': [deviceId],
      'play': play,
    };

    final response = await _client!.put(
      Uri.parse('$_apiBaseUrl/me/player'),
      headers: {'Content-Type': 'application/json'},
      body: jsonEncode(body),
    );

    if (response.statusCode != 204) {
      throw Exception('Failed to transfer playback: ${response.body}');
    }
  }

  // Search for tracks
  Future<Map<String, dynamic>> search(
    String query, {
    String type = 'track',
    int limit = 20,
  }) async {
    _ensureAuthenticated();

    final encodedQuery = Uri.encodeComponent(query);
    final response = await _client!.get(
      Uri.parse('$_apiBaseUrl/search?q=$encodedQuery&type=$type&limit=$limit'),
    );

    if (response.statusCode == 200) {
      return jsonDecode(response.body);
    } else {
      throw Exception('Failed to search: ${response.body}');
    }
  }

  // Utility methods
  void _ensureAuthenticated() {
    if (_client == null) {
      throw Exception('Not authenticated. Call authenticate() first.');
    }
  }

  String _generateRandomString(int length) {
    const chars =
        'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789';
    final random = Random();
    return String.fromCharCodes(
      Iterable.generate(
        length,
        (_) => chars.codeUnitAt(random.nextInt(chars.length)),
      ),
    );
  }

  void dispose() {
    _client?.close();
    _client = null;
  }
}
