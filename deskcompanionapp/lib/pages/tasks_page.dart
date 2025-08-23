import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:shared_preferences/shared_preferences.dart';

class TasksPage extends StatefulWidget {
  const TasksPage({Key? key}) : super(key: key);

  @override
  State<TasksPage> createState() => _TasksPageState();
}

class _TasksPageState extends State<TasksPage> {
  List<Map<String, dynamic>> tasks = [];

  @override
  void initState() {
    super.initState();
    _loadTasks();
  }

  Future<void> _loadTasks() async {
    final prefs = await SharedPreferences.getInstance();
    final String? data = prefs.getString('tasks');
    if (data != null) {
      setState(() {
        tasks = List<Map<String, dynamic>>.from(jsonDecode(data));
      });
    }
  }

  Future<void> _saveTasks() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString('tasks', jsonEncode(tasks));
  }

  void _addTask() {
    final nameController = TextEditingController();
    final durationController = TextEditingController();
    DateTime? deadline;

    showModalBottomSheet(
      isScrollControlled: true,
      context: context,
      builder: (BuildContext context) {
        return SizedBox(
          height: MediaQuery.of(context).size.height * 0.8,
          child: Padding(
            padding: const EdgeInsets.all(16.0),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                Text("Add Task", style: TextStyle(fontSize: 22)),
                TextField(
                  controller: nameController,
                  decoration: const InputDecoration(labelText: "Task Name"),
                ),
                TextField(
                  controller: durationController,
                  decoration: const InputDecoration(
                    labelText: "Duration (minutes)",
                  ),
                  keyboardType: TextInputType.number,
                ),
                const SizedBox(height: 16),
                ElevatedButton(
                  onPressed: () async {
                    // pick deadline
                    final picked = await showDatePicker(
                      context: context,
                      initialDate: DateTime.now(),
                      firstDate: DateTime(2020),
                      lastDate: DateTime(2100),
                    );
                    if (picked != null) {
                      deadline = picked;

                      // add task to the list
                      if (nameController.text.isNotEmpty &&
                          durationController.text.isNotEmpty &&
                          deadline != null) {
                        setState(() {
                          tasks.add({
                            "name": nameController.text,
                            "duration": durationController.text,
                            "deadline": deadline!.toIso8601String(),
                          });
                        });
                        _saveTasks();
                        if (context.mounted) Navigator.pop(context);
                      }
                    }
                  },
                  child: const Text("Pick Deadline & Add"),
                ),
              ],
            ),
          ),
        );
      },
    );
  }

  void _deleteTask(int index) {
    setState(() {
      tasks.removeAt(index);
    });
    _saveTasks();
  }

  /// Export tasks as JSON string (easy to send via BLE/WiFi to ESP32)
  String _exportTasks() {
    return jsonEncode(tasks);
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text("Task Manager"),
        actions: [
          IconButton(
            icon: const Icon(Icons.share),
            onPressed: () {
              final exported = _exportTasks();
              // For now just show in dialog
              showDialog(
                context: context,
                builder:
                    (_) => AlertDialog(
                      title: const Text("Export Data"),
                      content: SingleChildScrollView(child: Text(exported)),
                      actions: [
                        TextButton(
                          onPressed: () => Navigator.pop(context),
                          child: const Text("Close"),
                        ),
                      ],
                    ),
              );
              // Later: send this string to your ESP32 over BLE/WiFi
            },
          ),
        ],
      ),
      body: ListView.builder(
        itemCount: tasks.length,
        itemBuilder: (context, index) {
          final t = tasks[index];
          return ListTile(
            title: Text(t["name"]),
            subtitle: Text(
              "Deadline: ${t["deadline"].toString().substring(0, 10)} | Duration: ${t["duration"]}m",
            ),
            trailing: IconButton(
              icon: const Icon(Icons.delete),
              onPressed: () => _deleteTask(index),
            ),
          );
        },
      ),
      floatingActionButton: FloatingActionButton(
        onPressed: _addTask,
        child: const Icon(Icons.add),
      ),
    );
  }
}
