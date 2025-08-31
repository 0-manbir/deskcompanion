import 'package:flutter/material.dart';
import 'package:notification_listener_service/notification_listener_service.dart';
import 'package:provider/provider.dart';
import '../services/desk_companion_service.dart';

class NotificationsPage extends StatelessWidget {
  const NotificationsPage({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return Consumer<DeskCompanionService>(
      builder: (context, service, child) {
        return Column(
          children: [
            // Header with controls
            Container(
              padding: EdgeInsets.all(16),
              child: Column(
                children: [
                  Row(
                    children: [
                      Icon(Icons.notifications, size: 24),
                      SizedBox(width: 12),
                      Expanded(
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Text(
                              "Notifications",
                              style: TextStyle(
                                fontSize: 18,
                                fontWeight: FontWeight.bold,
                              ),
                            ),
                            Text(
                              "${service.recentNotifications.length} notifications received",
                              style: TextStyle(color: Colors.grey[600]),
                            ),
                          ],
                        ),
                      ),
                      Row(
                        children: [
                          Icon(
                            service.isConnected
                                ? Icons.bluetooth_connected
                                : Icons.bluetooth_disabled,
                            color:
                                service.isConnected ? Colors.green : Colors.red,
                            size: 20,
                          ),
                          SizedBox(width: 4),
                          Text(
                            service.isConnected ? "Synced" : "No sync",
                            style: TextStyle(
                              color:
                                  service.isConnected
                                      ? Colors.green
                                      : Colors.red,
                              fontSize: 12,
                            ),
                          ),
                        ],
                      ),
                    ],
                  ),
                  SizedBox(height: 12),

                  // Permission check
                  FutureBuilder<bool>(
                    future: NotificationListenerService.isPermissionGranted(),
                    builder: (context, snapshot) {
                      if (snapshot.data != true) {
                        return Card(
                          color: Colors.orange.shade50,
                          child: ListTile(
                            leading: Icon(Icons.warning, color: Colors.orange),
                            title: Text("Permission Required"),
                            subtitle: Text(
                              "Grant notification access to sync notifications",
                            ),
                            trailing: ElevatedButton(
                              onPressed: () async {
                                await NotificationListenerService.requestPermission();
                              },
                              child: Text("Grant"),
                            ),
                          ),
                        );
                      }
                      return SizedBox.shrink();
                    },
                  ),
                ],
              ),
            ),

            // Notifications List
            Expanded(
              child:
                  service.recentNotifications.isEmpty
                      ? Center(
                        child: Column(
                          mainAxisAlignment: MainAxisAlignment.center,
                          children: [
                            Icon(
                              Icons.notifications_off,
                              size: 64,
                              color: Colors.grey,
                            ),
                            SizedBox(height: 16),
                            Text(
                              "No notifications yet",
                              style: TextStyle(
                                fontSize: 18,
                                color: Colors.grey,
                              ),
                            ),
                            SizedBox(height: 8),
                            Text(
                              "Make sure notification access is granted",
                              style: TextStyle(color: Colors.grey[600]),
                            ),
                          ],
                        ),
                      )
                      : ListView.builder(
                        itemCount: service.recentNotifications.length,
                        itemBuilder: (context, index) {
                          final notification =
                              service.recentNotifications[index];
                          return Card(
                            margin: EdgeInsets.symmetric(
                              horizontal: 16,
                              vertical: 4,
                            ),
                            child: ListTile(
                              leading:
                                  notification.appIcon != null
                                      ? ClipOval(
                                        child: Image.memory(
                                          notification.appIcon!,
                                          width: 40,
                                          height: 40,
                                          fit: BoxFit.cover,
                                        ),
                                      )
                                      : CircleAvatar(
                                        backgroundColor: Colors.grey.shade200,
                                        child: Icon(Icons.notifications),
                                      ),
                              title: Text(
                                notification.title ?? "No title",
                                maxLines: 1,
                                overflow: TextOverflow.ellipsis,
                              ),
                              subtitle: Column(
                                crossAxisAlignment: CrossAxisAlignment.start,
                                children: [
                                  if (notification.content?.isNotEmpty == true)
                                    Text(
                                      notification.content!,
                                      maxLines: 2,
                                      overflow: TextOverflow.ellipsis,
                                    ),
                                  SizedBox(height: 4),
                                  Text(
                                    // _formatTimestamp(notification.createTime),
                                    // TODO create time is not defined
                                    "21m ago",
                                    style: TextStyle(
                                      fontSize: 12,
                                      color: Colors.grey,
                                    ),
                                  ),
                                ],
                              ),
                              isThreeLine: true,
                              trailing:
                                  notification.hasRemoved == true
                                      ? Icon(
                                        Icons.clear,
                                        color: Colors.red,
                                        size: 16,
                                      )
                                      : service.isConnected
                                      ? Icon(
                                        Icons.sync,
                                        color: Colors.green,
                                        size: 16,
                                      )
                                      : Icon(
                                        Icons.sync_disabled,
                                        color: Colors.grey,
                                        size: 16,
                                      ),
                            ),
                          );
                        },
                      ),
            ),
          ],
        );
      },
    );
  }

  String _formatTimestamp(int? timestamp) {
    if (timestamp == null) return "Unknown time";

    final now = DateTime.now();
    final notificationTime = DateTime.fromMillisecondsSinceEpoch(timestamp);
    final difference = now.difference(notificationTime);

    if (difference.inMinutes < 1) {
      return "Just now";
    } else if (difference.inMinutes < 60) {
      return "${difference.inMinutes}m ago";
    } else if (difference.inHours < 24) {
      return "${difference.inHours}h ago";
    } else {
      return "${difference.inDays}d ago";
    }
  }
}
