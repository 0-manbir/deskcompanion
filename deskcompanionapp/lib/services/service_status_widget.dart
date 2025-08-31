import 'package:deskcompanionapp/services/desk_companion_service.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

class ServiceStatusWidget extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return Consumer<DeskCompanionService>(
      builder: (context, service, child) {
        return Card(
          child: Padding(
            padding: EdgeInsets.all(16),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Row(
                  children: [
                    Icon(
                      service.isConnected
                          ? Icons.bluetooth_connected
                          : service.isScanning
                          ? Icons.bluetooth_searching
                          : Icons.bluetooth_disabled,
                      color:
                          service.isConnected
                              ? Colors.green
                              : service.isScanning
                              ? Colors.blue
                              : Colors.red,
                    ),
                    SizedBox(width: 8),
                    Text(
                      service.isConnected
                          ? "Connected"
                          : service.isScanning
                          ? "Searching..."
                          : "Disconnected",
                      style: TextStyle(
                        fontWeight: FontWeight.bold,
                        color: service.isConnected ? Colors.green : Colors.red,
                      ),
                    ),
                    Spacer(),
                    if (!service.isConnected)
                      ElevatedButton(
                        onPressed: service.forceReconnect,
                        child: Text("Reconnect"),
                      ),
                  ],
                ),
                SizedBox(height: 12),

                // Music Status
                if (service.currentPlayerState?.track != null) ...[
                  Text(
                    "🎵 Now Playing:",
                    style: TextStyle(fontWeight: FontWeight.bold),
                  ),
                  Text("${service.currentPlayerState!.track!.name}"),
                  Text("by ${service.currentPlayerState!.track!.artist.name}"),
                  SizedBox(height: 8),
                ],

                // Recent Notifications
                Text(
                  "📱 Recent Notifications: ${service.recentNotifications.length}",
                ),
                SizedBox(height: 8),

                // Connection Log (collapsible)
                ExpansionTile(
                  title: Text("Connection Log"),
                  children: [
                    Container(
                      height: 200,
                      child: SingleChildScrollView(
                        child: Text(
                          service.connectionLog,
                          style: TextStyle(
                            fontSize: 12,
                            fontFamily: 'monospace',
                          ),
                        ),
                      ),
                    ),
                    ElevatedButton(
                      onPressed: service.clearLog,
                      child: Text("Clear Log"),
                    ),
                  ],
                ),
              ],
            ),
          ),
        );
      },
    );
  }
}
