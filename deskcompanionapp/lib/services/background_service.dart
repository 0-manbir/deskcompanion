import 'dart:async';
import 'dart:ui';
import 'package:deskcompanionapp/services/desk_companion_service.dart';
import 'package:flutter/material.dart';
import 'package:flutter_background_service/flutter_background_service.dart';

class BackgroundService {
  @pragma('vm:entry-point')
  static Future<void> initializeService() async {
    final service = FlutterBackgroundService();

    await service.configure(
      androidConfiguration: AndroidConfiguration(
        onStart: onStart, // <-- now pointing to top-level function
        autoStart: true,
        isForegroundMode: true,
        notificationChannelId: 'desk_companion_bg',
        initialNotificationTitle: 'Desk Companion',
        initialNotificationContent: 'Syncing with your desk companion...',
        foregroundServiceNotificationId: 888,
      ),
      iosConfiguration: IosConfiguration(
        autoStart: true,
        onForeground: onStart,
        onBackground: onIosBackground,
      ),
    );
  }
}

/// === TOP LEVEL FUNCTIONS ===

@pragma('vm:entry-point')
void onStart(ServiceInstance service) async {
  DartPluginRegistrant.ensureInitialized();

  if (service is AndroidServiceInstance) {
    service.on('setAsForeground').listen((event) {
      service.setAsForegroundService();
    });

    service.on('setAsBackground').listen((event) {
      service.setAsBackgroundService();
    });
  }

  service.on('stopService').listen((event) {
    service.stopSelf();
  });

  // Initialize the desk companion service in background
  final deskService = DeskCompanionService();
  await deskService.initialize();

  // Periodic status update
  Timer.periodic(const Duration(seconds: 30), (timer) async {
    if (service is AndroidServiceInstance) {
      if (await service.isForegroundService()) {
        service.setForegroundNotificationInfo(
          title: "Desk Companion",
          content:
              deskService.isConnected
                  ? "Connected and syncing..."
                  : "Searching for device...",
        );
      }
    }
  });
}

@pragma('vm:entry-point')
Future<bool> onIosBackground(ServiceInstance service) async {
  WidgetsFlutterBinding.ensureInitialized();
  DartPluginRegistrant.ensureInitialized();
  return true;
}
