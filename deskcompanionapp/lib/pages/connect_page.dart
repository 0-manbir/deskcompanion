import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../services/desk_companion_service.dart';

class ConnectPage extends StatelessWidget {
  const ConnectPage({super.key});

  @override
  Widget build(BuildContext context) {
    return Consumer<DeskCompanionService>(
      builder: (context, service, child) {
        return Scaffold(
          body: Padding(
            padding: const EdgeInsets.all(16),
            child: Column(
              children: [
                // Connection Status Card
                Card(
                  child: Padding(
                    padding: EdgeInsets.all(20),
                    child: Column(
                      children: [
                        Icon(
                          service.isConnected
                              ? Icons.bluetooth_connected
                              : service.isScanning
                              ? Icons.bluetooth_searching
                              : Icons.bluetooth_disabled,
                          size: 64,
                          color:
                              service.isConnected
                                  ? Colors.green
                                  : service.isScanning
                                  ? Colors.blue
                                  : Colors.red,
                        ),
                        SizedBox(height: 16),
                        Text(
                          service.isConnected
                              ? "Connected to Desk Companion"
                              : service.isScanning
                              ? "Searching for Desk Companion..."
                              : "Disconnected",
                          style: TextStyle(
                            fontSize: 18,
                            fontWeight: FontWeight.bold,
                          ),
                          textAlign: TextAlign.center,
                        ),
                        SizedBox(height: 8),
                        Text(
                          service.isConnected
                              ? "Your desk companion is connected and syncing"
                              : service.isScanning
                              ? "Make sure your ESP32 is powered on"
                              : "Connection will retry automatically",
                          textAlign: TextAlign.center,
                          style: TextStyle(color: Colors.grey[600]),
                        ),
                        SizedBox(height: 20),

                        // Action Buttons
                        Row(
                          mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                          children: [
                            ElevatedButton.icon(
                              onPressed:
                                  service.isScanning
                                      ? null
                                      : service.forceReconnect,
                              icon: Icon(Icons.refresh),
                              label: Text(
                                service.isScanning
                                    ? "Scanning..."
                                    : "Reconnect",
                              ),
                            ),
                            ElevatedButton.icon(
                              onPressed: () {
                                service.sendToESP32("PING");
                              },
                              icon: Icon(Icons.send),
                              label: Text("Test"),
                            ),
                          ],
                        ),
                      ],
                    ),
                  ),
                ),

                SizedBox(height: 20),

                // Manual Send Section
                Card(
                  child: Padding(
                    padding: EdgeInsets.all(16),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          "Manual Commands",
                          style: TextStyle(fontWeight: FontWeight.bold),
                        ),
                        SizedBox(height: 12),
                        Wrap(
                          spacing: 8,
                          children: [
                            _CommandChip(
                              "PING",
                              () => service.sendToESP32("PING"),
                            ),
                            _CommandChip("TIME SYNC", () async {
                              DateTime now = DateTime.now();
                              String timeString =
                                  "TIME:${now.hour.toString().padLeft(2, '0')}:${now.minute.toString().padLeft(2, '0')}:${now.second.toString().padLeft(2, '0')}";
                              service.sendToESP32(timeString);
                            }),
                            _CommandChip(
                              "TEST MUSIC",
                              () => service.sendToESP32(
                                "MUSIC:Test Song|Test Artist|true|75",
                              ),
                            ),
                            _CommandChip(
                              "TEST NOTIFICATION",
                              () => service.sendToESP32(
                                "NOTIFICATION:TestApp|Test Title|This is a test notification",
                              ),
                            ),
                          ],
                        ),
                      ],
                    ),
                  ),
                ),

                SizedBox(height: 20),

                // Connection Log
                Expanded(
                  child: Card(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Padding(
                          padding: EdgeInsets.all(16),
                          child: Row(
                            children: [
                              Text(
                                "Connection Log",
                                style: TextStyle(fontWeight: FontWeight.bold),
                              ),
                              Spacer(),
                              TextButton(
                                onPressed: service.clearLog,
                                child: Text("Clear"),
                              ),
                            ],
                          ),
                        ),
                        Expanded(
                          child: Container(
                            width: double.infinity,
                            padding: EdgeInsets.symmetric(horizontal: 16),
                            child: SingleChildScrollView(
                              child: Text(
                                service.connectionLog.isEmpty
                                    ? "No log entries yet..."
                                    : service.connectionLog,
                                style: TextStyle(
                                  fontSize: 12,
                                  fontFamily: 'monospace',
                                ),
                              ),
                            ),
                          ),
                        ),
                      ],
                    ),
                  ),
                ),
              ],
            ),
          ),
        );
      },
    );
  }
}

class _CommandChip extends StatelessWidget {
  final String label;
  final VoidCallback onTap;

  const _CommandChip(this.label, this.onTap);

  @override
  Widget build(BuildContext context) {
    return ActionChip(label: Text(label), onPressed: onTap);
  }
}
