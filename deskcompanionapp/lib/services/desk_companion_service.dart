// lib/services/desk_companion_service.dart
import 'dart:async';
import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:flutter_dotenv/flutter_dotenv.dart';
import 'package:notification_listener_service/notification_event.dart';
import 'package:notification_listener_service/notification_listener_service.dart';
import 'package:spotify_sdk/models/player_state.dart';
import 'package:spotify_sdk/spotify_sdk.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:volume_controller/volume_controller.dart';

class DeskCompanionService extends ChangeNotifier {
  static final DeskCompanionService _instance =
      DeskCompanionService._internal();
  factory DeskCompanionService() => _instance;
  DeskCompanionService._internal();

  // BLE Configuration
  final String serviceUUID = "fa974e39-7cab-4fa2-8681-1d71f9fb73bc";
  final String rxUUID = "12e87612-7b69-4e34-a21b-d2303bfa2691"; // To ESP32
  final String txUUID = "2b50f752-b04e-4c9f-b188-aa7d5cf93dab"; // From ESP32

  // BLE State
  BluetoothDevice? _device;
  BluetoothCharacteristic? _rxCharacteristic;
  BluetoothCharacteristic? _txCharacteristic;
  bool _isConnected = false;
  bool _isScanning = false;

  // Service State
  bool _isInitialized = false;
  Timer? _reconnectionTimer;
  Timer? _syncTimer;

  // Data Streams
  StreamSubscription<ServiceNotificationEvent>? _notificationSubscription;
  StreamSubscription<PlayerState>? _musicSubscription;
  StreamSubscription<List<ScanResult>>? _scanSubscription;

  // Current Data State
  PlayerState? _currentPlayerState;
  final List<ServiceNotificationEvent> _recentNotifications = [];
  List<Map<String, dynamic>> _currentTasks = [];
  String _connectionLog = "";

  // Getters
  bool get isConnected => _isConnected;
  bool get isScanning => _isScanning;
  bool get isInitialized => _isInitialized;
  PlayerState? get currentPlayerState => _currentPlayerState;
  List<ServiceNotificationEvent> get recentNotifications =>
      _recentNotifications;
  String get connectionLog => _connectionLog;

  // Initialize the service
  Future<void> initialize() async {
    if (_isInitialized) return;

    _log("🚀 Initializing Desk Companion Service...");

    try {
      // Initialize Spotify
      await _initializeSpotify();

      // Initialize Notification Listener
      await _initializeNotificationListener();

      // Load saved tasks
      await _loadTasks();

      // Start BLE connection attempts
      _startAutoConnection();

      // Start periodic sync
      _startPeriodicSync();

      _isInitialized = true;
      _log("✅ Service initialized successfully");
      notifyListeners();
    } catch (e) {
      _log("❌ Service initialization failed: $e");
    }
  }

  // Dispose the service
  @override
  void dispose() {
    _reconnectionTimer?.cancel();
    _syncTimer?.cancel();
    _notificationSubscription?.cancel();
    _musicSubscription?.cancel();
    _scanSubscription?.cancel();
    _device?.disconnect();
    super.dispose();
  }

  // === BLE Connection Management ===

  void _startAutoConnection() {
    _reconnectionTimer = Timer.periodic(Duration(seconds: 10), (timer) {
      if (!_isConnected && !_isScanning) {
        _scanAndConnect();
      }
    });

    // Initial connection attempt
    _scanAndConnect();
  }

  Future<void> _scanAndConnect() async {
    if (_isScanning || _isConnected) return;

    _isScanning = true;
    notifyListeners();
    _log("🔍 Scanning for Desk Companion...");

    try {
      // Start scanning
      await FlutterBluePlus.startScan(timeout: Duration(seconds: 5));

      _scanSubscription?.cancel();
      _scanSubscription = FlutterBluePlus.scanResults.listen((results) async {
        for (ScanResult result in results) {
          if (result.advertisementData.serviceUuids.toString().contains(
            serviceUUID,
          )) {
            FlutterBluePlus.stopScan();
            _log(
              "📱 Found device: ${result.advertisementData.serviceUuids.toString()}",
            );
            await FlutterBluePlus.stopScan();
            await _connectToDevice(result.device);
            break;
          }
        }
      });

      // Stop scanning after timeout
      await Future.delayed(Duration(seconds: 10));
      if (_isScanning) {
        await FlutterBluePlus.stopScan();
        _isScanning = false;
        if (!_isConnected) {
          _log("⏱️ Scan timeout - will retry");
        }
        notifyListeners();
      }
    } catch (e) {
      _log("❌ Scan error: $e");
      _isScanning = false;
      notifyListeners();
    }
  }

  Future<void> _connectToDevice(BluetoothDevice device) async {
    try {
      _log("🔗 Connecting to ${device.platformName}...");

      _device = device;

      // Listen for disconnection
      device.connectionState.listen((state) {
        if (state == BluetoothConnectionState.disconnected) {
          _log("❌ Device disconnected");
          _isConnected = false;
          _rxCharacteristic = null;
          _txCharacteristic = null;
          notifyListeners();
        }
      });

      await device.connect(timeout: Duration(seconds: 15));
      _log("✅ Connected to ${device.platformName}");

      // Discover services
      List<BluetoothService> services = await device.discoverServices();
      await _setupCharacteristics(services);

      _isConnected = true;
      _isScanning = false;
      notifyListeners();

      // Send initial data
      await _sendInitialData();
    } catch (e) {
      _log("❌ Connection failed: $e");
      _isScanning = false;
      notifyListeners();
    }
  }

  Future<void> _setupCharacteristics(List<BluetoothService> services) async {
    for (BluetoothService service in services) {
      if (service.uuid.toString().toLowerCase() == serviceUUID.toLowerCase()) {
        for (BluetoothCharacteristic characteristic
            in service.characteristics) {
          String charUUID = characteristic.uuid.toString().toLowerCase();

          if (charUUID == rxUUID.toLowerCase()) {
            _rxCharacteristic = characteristic;
            _log("📤 RX Characteristic ready");
          } else if (charUUID == txUUID.toLowerCase()) {
            _txCharacteristic = characteristic;
            await characteristic.setNotifyValue(true);

            // Listen for incoming data
            characteristic.lastValueStream.listen((value) {
              _handleIncomingData(utf8.decode(value));
            });

            _log("📥 TX Characteristic ready");
          }
        }
      }
    }
  }

  DateTime _lastSkipTime = DateTime.now();
  final Duration _skipCooldown = Duration(milliseconds: 600);

  void _handleIncomingData(String data) {
    _log("📩 Received: $data");

    if (data.startsWith("STATUS:")) {
      // Handle status updates from ESP32
    }

    if (data.startsWith("MUSIC")) {
      if (data.startsWith("MUSIC_VOLUME")) {
        VolumeController.instance.setVolume(
          double.parse(data.split(":")[1]) / 100.0,
        );
      } else if (data == "MUSIC_PLAY" || data == "MUSIC_PAUSE") {
        playPause();
      } else if (data == "MUSIC_NEXT") {
        _safeSkip(skipNext);
      } else if (data == "MUSIC_PREV") {
        _safeSkip(skipPrevious);
      } else if (data.startsWith("MUSIC_SEEK_RELATIVE")) {
        int millis = int.parse(data.split(":")[1]);
        seekRelative(millis);
      }
    }
  }

  void _safeSkip(Function action) {
    final now = DateTime.now();
    if (now.difference(_lastSkipTime) > _skipCooldown) {
      action();
      _lastSkipTime = now;
    } else {
      _log("⏱ Skip ignored (cooldown active)");
    }
  }

  // === Data Sending ===

  Future<void> sendToESP32(String data) async {
    if (_rxCharacteristic == null || !_isConnected) {
      _log("❌ Cannot send: Not connected");
      return;
    }

    try {
      await _rxCharacteristic!.write(utf8.encode(data), withoutResponse: true);
      _log("📤 Sent: $data");
    } catch (e) {
      _log("❌ Send error: $e");
    }
  }

  Future<void> _sendInitialData() async {
    // Send current time
    await _sendCurrentTime();

    // Send current music state if available
    if (_currentPlayerState != null) {
      await _sendMusicUpdate(_currentPlayerState!);
    }

    // Send tasks
    await _sendTasks();

    // Send recent notifications
    await _sendRecentNotifications();
  }

  Future<void> _sendCurrentTime() async {
    DateTime now = DateTime.now();
    String timeString =
        "TIME:${now.hour.toString().padLeft(2, '0')}:${now.minute.toString().padLeft(2, '0')}:${now.second.toString().padLeft(2, '0')}";
    await sendToESP32(timeString);
  }

  Future<void> _sendMusicUpdate(PlayerState playerState) async {
    String songName = playerState.track?.name ?? "Unknown";
    String artistName = playerState.track?.artist.name ?? "Unknown";
    String albumName = playerState.track?.album.name ?? "Unknown";

    int songDuration = playerState.track?.duration ?? 0;
    int playbackPosition = playerState.playbackPosition;

    // bool isShuffling = playerState.playbackOptions.isShuffling;
    // bool isPodcast = playerState.track?.isPodcast ?? false;
    // double playbackSpeed = playerState.playbackSpeed;
    // TODO podcast options
    bool isPlaying = !playerState.isPaused;
    int volume = (await VolumeController.instance.getVolume() * 100).toInt();

    String musicData =
        "MUSIC:$songName|$albumName|$artistName|$isPlaying|$volume|$songDuration|$playbackPosition";
    await sendToESP32(musicData);
  }

  Future<void> _sendNotificationUpdate(
    ServiceNotificationEvent notification,
  ) async {
    String app = notification.packageName ?? "Unknown";
    String title = notification.title ?? "";
    String content = notification.content ?? "";

    String notifData = "NOTIFICATION:$app|$title|$content";
    await sendToESP32(notifData);
  }

  Future<void> _sendTasks() async {
    if (_currentTasks.isNotEmpty) {
      List<String> taskStrings =
          _currentTasks.map((task) {
            return "${task['name']}(${task['duration']}m)";
          }).toList();

      String tasksData = "EVENTS:${taskStrings.join('|')}";
      await sendToESP32(tasksData);
    }
  }

  Future<void> _sendRecentNotifications() async {
    // Send last 5 notifications
    List<ServiceNotificationEvent> recent =
        _recentNotifications.take(5).toList();
    for (var notification in recent) {
      await _sendNotificationUpdate(notification);
      await Future.delayed(
        Duration(milliseconds: 100),
      ); // Small delay between sends
    }
  }

  // === Spotify Integration ===

  Future<void> _initializeSpotify() async {
    try {
      // Authenticate
      await SpotifySdk.getAccessToken(
        clientId: dotenv.env['CLIENT_ID'].toString(),
        redirectUrl: dotenv.env['REDIRECT_URL'].toString(),
        scope:
            "app-remote-control,user-modify-playback-state,user-read-playback-state,user-read-currently-playing",
      );

      // Connect
      await SpotifySdk.connectToSpotifyRemote(
        clientId: dotenv.env['CLIENT_ID'].toString(),
        redirectUrl: dotenv.env['REDIRECT_URL'].toString(),
      );

      // Subscribe to player state
      _musicSubscription?.cancel();
      _musicSubscription = SpotifySdk.subscribePlayerState().listen(
        (playerState) {
          _currentPlayerState = playerState;
          if (_isConnected) {
            _sendMusicUpdate(playerState);
          }
          notifyListeners();
        },
        onError: (error) {
          _log("🎵 Music subscription error: $error");
        },
      );

      _log("🎵 Spotify initialized");
    } catch (e) {
      _log("❌ Spotify initialization failed: $e");
    }
  }

  // Music control methods
  Future<void> playPause() async {
    try {
      if (_currentPlayerState?.isPaused == true) {
        await SpotifySdk.resume();
      } else {
        await SpotifySdk.pause();
      }
    } catch (e) {
      _log("❌ Play/Pause error: $e");
    }
  }

  Future<void> skipNext() async {
    try {
      await SpotifySdk.skipNext();
    } catch (e) {
      _log("❌ Skip next error: $e");
    }
  }

  Future<void> skipPrevious() async {
    try {
      await SpotifySdk.skipPrevious();
    } catch (e) {
      _log("❌ Skip previous error: $e");
    }
  }

  Future<void> seekRelative(int millis) async {
    try {
      await SpotifySdk.seekToRelativePosition(relativeMilliseconds: millis);
    } catch (e) {
      _log("❌ Relative seek error: $e");
    }
  }

  // === Notification Listener ===

  Future<void> _initializeNotificationListener() async {
    try {
      bool hasPermission =
          await NotificationListenerService.isPermissionGranted();

      if (!hasPermission) {
        _log("📱 Requesting notification permission...");
        await NotificationListenerService.requestPermission();
        return;
      }

      _startNotificationListener();
      _log("📱 Notification listener initialized");
    } catch (e) {
      _log("❌ Notification listener initialization failed: $e");
    }
  }

  void _startNotificationListener() {
    _notificationSubscription?.cancel();
    _notificationSubscription = NotificationListenerService.notificationsStream
        .listen((notification) {
          // Filter out unwanted notifications
          if (_shouldIgnoreNotification(notification)) {
            return;
          }

          _log("📱 New notification: ${notification.title}");

          // Add to recent notifications
          _recentNotifications.insert(0, notification);
          if (_recentNotifications.length > 20) {
            _recentNotifications.removeLast();
          }

          // Send to ESP32 if connected
          if (_isConnected) {
            _sendNotificationUpdate(notification);
          }

          notifyListeners();
        });
  }

  bool _shouldIgnoreNotification(ServiceNotificationEvent notification) {
    return false;
    String? packageName = notification.packageName?.toLowerCase();
    String? title = notification.title?.toLowerCase();

    // Ignore system notifications and our own app
    List<String> ignoredPackages = [
      'android',
      'com.android.systemui',
      'com.yourapp.deskcompanionapp', // Replace with your package name
    ];

    if (packageName != null &&
        ignoredPackages.any((pkg) => packageName.contains(pkg))) {
      return true;
    }

    // Ignore empty notifications
    if ((title?.isEmpty ?? true) && (notification.content?.isEmpty ?? true)) {
      return true;
    }

    return false;
  }

  // === Tasks Management ===

  Future<void> _loadTasks() async {
    try {
      final prefs = await SharedPreferences.getInstance();
      final String? data = prefs.getString('tasks');
      if (data != null) {
        _currentTasks = List<Map<String, dynamic>>.from(jsonDecode(data));
        _log("📋 Loaded ${_currentTasks.length} tasks");
      }
    } catch (e) {
      _log("❌ Task loading error: $e");
    }
  }

  Future<void> updateTasks(List<Map<String, dynamic>> tasks) async {
    _currentTasks = tasks;
    if (_isConnected) {
      await _sendTasks();
    }
    notifyListeners();
  }

  // === Periodic Sync ===

  void _startPeriodicSync() {
    _syncTimer = Timer.periodic(Duration(minutes: 1), (timer) {
      if (_isConnected) {
        _sendCurrentTime();
      }
    });
  }

  // === Utility ===

  void _log(String message) {
    String timestamp = DateTime.now().toString().substring(11, 19);
    String logEntry = "[$timestamp] $message";
    _connectionLog += "$logEntry\n";

    // Keep log size manageable
    List<String> lines = _connectionLog.split('\n');
    if (lines.length > 100) {
      _connectionLog = lines.sublist(lines.length - 50).join('\n');
    }

    print(logEntry); // Also print to console
    notifyListeners();
  }

  void clearLog() {
    _connectionLog = "";
    notifyListeners();
  }

  // Manual connection trigger
  Future<void> forceReconnect() async {
    if (_device != null) {
      await _device!.disconnect();
    }
    _isConnected = false;
    _scanAndConnect();
  }
}
