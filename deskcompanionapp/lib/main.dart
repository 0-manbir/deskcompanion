// lib/main.dart
import 'package:deskcompanionapp/services/service_status_widget.dart';
import 'package:flutter/material.dart';
import 'package:flutter_dotenv/flutter_dotenv.dart';
import 'package:notification_listener_service/notification_listener_service.dart';
import 'package:provider/provider.dart';
import 'package:flutter_background_service/flutter_background_service.dart';

import 'services/desk_companion_service.dart';
import 'services/background_service.dart';
import 'pages/connect_page.dart';
import 'pages/music_controls.dart';
import 'pages/notifications_page.dart';
import 'pages/tasks_page.dart';
import 'package:deskcompanionapp/services/desk_companion_service.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();

  await dotenv.load(fileName: ".env");

  // Initialize background service
  await BackgroundService.initializeService();

  runApp(MyApp());
}

class MyApp extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return ChangeNotifierProvider(
      create: (context) => DeskCompanionService(),
      child: MaterialApp(
        title: 'Desk Companion',
        theme: ThemeData(primarySwatch: Colors.blue, useMaterial3: true),
        home: HomeScreen(),
      ),
    );
  }
}

class HomeScreen extends StatefulWidget {
  @override
  _HomeScreenState createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  int selectedIndex = 0;
  late DeskCompanionService _service;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) {
      _service = Provider.of<DeskCompanionService>(context, listen: false);
      _service.initialize();
    });
  }

  final List<Widget> pages = [
    DashboardPage(),
    NotificationsPage(),
    TasksPage(),
    MusicControls(),
    SettingsPage(),
  ];

  final List<String> titles = const [
    "Dashboard",
    "Notifications",
    "Tasks",
    "Music",
    "Settings",
  ];

  void onItemTapped(int index) {
    setState(() {
      selectedIndex = index;
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text(titles[selectedIndex]), elevation: 0),
      body: pages[selectedIndex],
      bottomNavigationBar: BottomNavigationBar(
        currentIndex: selectedIndex,
        onTap: onItemTapped,
        type: BottomNavigationBarType.fixed,
        items: const [
          BottomNavigationBarItem(
            icon: Icon(Icons.dashboard),
            label: 'Dashboard',
          ),
          BottomNavigationBarItem(
            icon: Icon(Icons.notifications),
            label: 'Notifications',
          ),
          BottomNavigationBarItem(icon: Icon(Icons.task_alt), label: 'Tasks'),
          BottomNavigationBarItem(icon: Icon(Icons.music_note), label: 'Music'),
          BottomNavigationBarItem(
            icon: Icon(Icons.settings),
            label: 'Settings',
          ),
        ],
      ),
    );
  }
}

// New Dashboard Page
class DashboardPage extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return SingleChildScrollView(
      padding: EdgeInsets.all(16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // Service Status Card
          ServiceStatusWidget(),
          SizedBox(height: 16),

          // Quick Actions
          Card(
            child: Padding(
              padding: EdgeInsets.all(16),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    "Quick Actions",
                    style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
                  ),
                  SizedBox(height: 12),
                  Row(
                    mainAxisAlignment: MainAxisAlignment.spaceAround,
                    children: [
                      _QuickActionButton(
                        icon: Icons.play_circle,
                        label: "Play/Pause",
                        onTap:
                            () =>
                                Provider.of<DeskCompanionService>(
                                  context,
                                  listen: false,
                                ).playPause(),
                      ),
                      _QuickActionButton(
                        icon: Icons.skip_next,
                        label: "Next Song",
                        onTap:
                            () =>
                                Provider.of<DeskCompanionService>(
                                  context,
                                  listen: false,
                                ).skipNext(),
                      ),
                      _QuickActionButton(
                        icon: Icons.refresh,
                        label: "Reconnect",
                        onTap:
                            () =>
                                Provider.of<DeskCompanionService>(
                                  context,
                                  listen: false,
                                ).forceReconnect(),
                      ),
                    ],
                  ),
                ],
              ),
            ),
          ),
          SizedBox(height: 16),

          // Current Music
          Consumer<DeskCompanionService>(
            builder: (context, service, child) {
              if (service.currentPlayerState?.track != null) {
                final track = service.currentPlayerState!.track!;
                return Card(
                  child: ListTile(
                    leading: Icon(Icons.music_note, size: 40),
                    title: Text(
                      track.name,
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                    ),
                    subtitle: Text("by ${track.artist.name}"),
                    trailing: Icon(
                      service.currentPlayerState!.isPaused
                          ? Icons.pause_circle
                          : Icons.play_circle,
                    ),
                  ),
                );
              }
              return Card(
                child: ListTile(
                  leading: Icon(Icons.music_off, size: 40),
                  title: Text("No music playing"),
                  subtitle: Text("Open Spotify to start"),
                ),
              );
            },
          ),
          SizedBox(height: 16),

          // Recent Notifications Summary
          Consumer<DeskCompanionService>(
            builder: (context, service, child) {
              return Card(
                child: Padding(
                  padding: EdgeInsets.all(16),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        "Recent Notifications",
                        style: TextStyle(
                          fontSize: 16,
                          fontWeight: FontWeight.bold,
                        ),
                      ),
                      SizedBox(height: 8),
                      Text(
                        "${service.recentNotifications.length} notifications received",
                      ),
                      if (service.recentNotifications.isNotEmpty) ...[
                        SizedBox(height: 8),
                        ...service.recentNotifications
                            .take(3)
                            .map(
                              (notification) => ListTile(
                                dense: true,
                                title: Text(
                                  notification.title ?? "No title",
                                  maxLines: 1,
                                  overflow: TextOverflow.ellipsis,
                                ),
                                subtitle: Text(
                                  notification.content ?? "",
                                  maxLines: 1,
                                  overflow: TextOverflow.ellipsis,
                                ),
                                leading:
                                    notification.appIcon != null
                                        ? Image.memory(
                                          notification.appIcon!,
                                          width: 24,
                                          height: 24,
                                        )
                                        : Icon(Icons.notifications),
                              ),
                            ),
                      ],
                    ],
                  ),
                ),
              );
            },
          ),
        ],
      ),
    );
  }
}

class _QuickActionButton extends StatelessWidget {
  final IconData icon;
  final String label;
  final VoidCallback onTap;

  const _QuickActionButton({
    required this.icon,
    required this.label,
    required this.onTap,
  });

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTap: onTap,
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          Container(
            padding: EdgeInsets.all(12),
            decoration: BoxDecoration(
              color: Theme.of(context).primaryColor.withOpacity(0.1),
              borderRadius: BorderRadius.circular(12),
            ),
            child: Icon(icon, size: 24, color: Theme.of(context).primaryColor),
          ),
          SizedBox(height: 8),
          Text(
            label,
            style: TextStyle(fontSize: 12),
            textAlign: TextAlign.center,
          ),
        ],
      ),
    );
  }
}

// Settings Page
class SettingsPage extends StatelessWidget {
  const SettingsPage({super.key});

  @override
  Widget build(BuildContext context) {
    return Consumer<DeskCompanionService>(
      builder: (context, service, child) {
        return ListView(
          padding: EdgeInsets.all(16),
          children: [
            Card(
              child: ListTile(
                leading: Icon(Icons.bluetooth),
                title: Text("BLE Connection"),
                subtitle: Text(
                  service.isConnected ? "Connected" : "Disconnected",
                ),
                trailing: Switch(
                  value: service.isConnected,
                  onChanged:
                      service.isConnected
                          ? null
                          : (value) => service.forceReconnect(),
                ),
              ),
            ),

            Card(
              child: ListTile(
                leading: Icon(Icons.notifications),
                title: Text("Notification Access"),
                subtitle: Text("Allow app to read notifications"),
                onTap: () async {
                  // Open notification access settings
                  await NotificationListenerService.requestPermission();
                },
              ),
            ),

            Card(
              child: ListTile(
                leading: Icon(Icons.music_note),
                title: Text("Spotify Integration"),
                subtitle: Text("Connect to Spotify for music control"),
                onTap: () {
                  // Navigate to music page or reinitialize
                  Navigator.push(
                    context,
                    MaterialPageRoute(builder: (context) => MusicControls()),
                  );
                },
              ),
            ),

            Card(
              child: ListTile(
                leading: Icon(Icons.delete_sweep),
                title: Text("Clear Connection Log"),
                subtitle: Text("Clear debugging information"),
                onTap: service.clearLog,
              ),
            ),

            SizedBox(height: 20),

            // Background Service Control
            Card(
              child: Padding(
                padding: EdgeInsets.all(16),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      "Background Service",
                      style: TextStyle(fontWeight: FontWeight.bold),
                    ),
                    SizedBox(height: 8),
                    Text(
                      "The background service keeps your desk companion synced even when the app is closed.",
                    ),
                    SizedBox(height: 12),
                    Row(
                      children: [
                        ElevatedButton(
                          onPressed: () {
                            FlutterBackgroundService().invoke(
                              "setAsForeground",
                            );
                          },
                          child: Text("Enable BG"),
                        ),
                        SizedBox(width: 12),
                        ElevatedButton(
                          style: ElevatedButton.styleFrom(
                            backgroundColor: Colors.red[500],
                          ),
                          onPressed: () {
                            FlutterBackgroundService().invoke("stopService");
                          },
                          child: Text(
                            "Stop",
                            style: TextStyle(color: Colors.white),
                          ),
                        ),
                      ],
                    ),
                  ],
                ),
              ),
            ),

            // Debug Info
            Card(
              child: ExpansionTile(
                leading: Icon(Icons.bug_report),
                title: Text("Debug Information"),
                children: [
                  Padding(
                    padding: EdgeInsets.all(16),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          "Service Status: ${service.isInitialized ? 'Initialized' : 'Not Initialized'}",
                        ),
                        Text(
                          "BLE Status: ${service.isConnected ? 'Connected' : 'Disconnected'}",
                        ),
                        Text(
                          "Notifications: ${service.recentNotifications.length} recent",
                        ),
                        Text(
                          "Music State: ${service.currentPlayerState != null ? 'Available' : 'Unavailable'}",
                        ),
                      ],
                    ),
                  ),
                ],
              ),
            ),
          ],
        );
      },
    );
  }
}
