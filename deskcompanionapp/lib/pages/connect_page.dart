import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:permission_handler/permission_handler.dart';

class ConnectPage extends StatefulWidget {
  const ConnectPage({super.key});

  @override
  State<ConnectPage> createState() => _ConnectPageState();
}

class _ConnectPageState extends State<ConnectPage> {
  final String serviceUUID = "fa974e39-7cab-4fa2-8681-1d71f9fb73bc";

  final String rxUUID = "12e87612-7b69-4e34-a21b-d2303bfa2691";
  // to ESP32
  final String txUUID = "2b50f752-b04e-4c9f-b188-aa7d5cf93dab";
  // from ESP32
  BluetoothDevice? _device;

  BluetoothCharacteristic? _rx;

  BluetoothCharacteristic? _tx;

  String _log = "";

  @override
  void initState() {
    super.initState();
    requestPermissions();
  }

  void requestPermissions() async {
    await Permission.bluetooth.request();
    await Permission.location.request();
    scanAndConnect();
  }

  void scanAndConnect() async {
    FlutterBluePlus.startScan(timeout: Duration(seconds: 5));
    FlutterBluePlus.scanResults.listen((results) async {
      for (ScanResult r in results) {
        if (r.advertisementData.serviceUuids.toString().contains(serviceUUID)) {
          FlutterBluePlus.stopScan();
          _device = r.device;
          await _device!.connect();
          await _device!.discoverServices();
          discoverServices();
          break;
        }
      }
    });
  }

  void discoverServices() async {
    if (_device == null) return;

    List<BluetoothService> services = await _device!.discoverServices();
    for (var s in services) {
      if (s.uuid.toString() == serviceUUID) {
        for (var c in s.characteristics) {
          if (c.uuid.toString() == rxUUID) {
            _rx = c;
          } else if (c.uuid.toString() == txUUID) {
            _tx = c;
            await _tx!.setNotifyValue(true);
            _tx!.lastValueStream.listen((value) {
              if (context.mounted) {
                setState(() {
                  _log += "\nESP32: ${utf8.decode(value)}";
                });
              }
            });
          }
        }
      }
    }
  }

  void sendToESP32(String text) async {
    if (_rx == null) {
      print("rx is null");
      return;
    }
    await _rx!.write(utf8.encode(text), withoutResponse: true);
    if (context.mounted) {
      setState(() {
        _log += "\nApp: $text";
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text("ESP32 BLE")),
      body: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          children: [
            ElevatedButton(
              onPressed: () => sendToESP32("PING"),
              child: Text("Send PING"),
            ),
            ElevatedButton(
              onPressed: () {
                setState(() {
                  _log = "";
                });
              },
              child: Text("Clear Log"),
            ),
            SizedBox(height: 20),
            Expanded(child: SingleChildScrollView(child: Text(_log))),
          ],
        ),
      ),
    );
  }
}
