// lib/pages/updated_tasks_page.dart
import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:shared_preferences/shared_preferences.dart';
import '../services/desk_companion_service.dart';

class TasksPage extends StatefulWidget {
  const TasksPage({super.key});

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

    // Update the background service
    final service = Provider.of<DeskCompanionService>(context, listen: false);
    await service.updateTasks(tasks);
  }

  void _addTask() {
    final nameController = TextEditingController();
    final durationController = TextEditingController();
    DateTime? deadline;

    showModalBottomSheet(
      isScrollControlled: true,
      context: context,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.vertical(top: Radius.circular(20)),
      ),
      builder: (BuildContext context) {
        return StatefulBuilder(
          builder: (context, setModalState) {
            return Container(
              padding: EdgeInsets.only(
                bottom: MediaQuery.of(context).viewInsets.bottom,
              ),
              child: Padding(
                padding: const EdgeInsets.all(24.0),
                child: Column(
                  mainAxisSize: MainAxisSize.min,
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      "Add New Task",
                      style: TextStyle(
                        fontSize: 24,
                        fontWeight: FontWeight.bold,
                      ),
                    ),
                    SizedBox(height: 20),

                    TextField(
                      controller: nameController,
                      decoration: InputDecoration(
                        labelText: "Task Name",
                        border: OutlineInputBorder(
                          borderRadius: BorderRadius.circular(12),
                        ),
                      ),
                    ),
                    SizedBox(height: 16),

                    TextField(
                      controller: durationController,
                      decoration: InputDecoration(
                        labelText: "Duration (minutes)",
                        border: OutlineInputBorder(
                          borderRadius: BorderRadius.circular(12),
                        ),
                      ),
                      keyboardType: TextInputType.number,
                    ),
                    SizedBox(height: 16),

                    Container(
                      width: double.infinity,
                      padding: EdgeInsets.all(16),
                      decoration: BoxDecoration(
                        border: Border.all(color: Colors.grey.shade300),
                        borderRadius: BorderRadius.circular(12),
                      ),
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text(
                            "Deadline",
                            style: TextStyle(color: Colors.grey[600]),
                          ),
                          SizedBox(height: 8),
                          Text(
                            deadline != null
                                ? "${deadline!.day}/${deadline!.month}/${deadline!.year}"
                                : "No deadline set",
                            style: TextStyle(fontSize: 16),
                          ),
                        ],
                      ),
                    ),
                    SizedBox(height: 20),

                    Row(
                      children: [
                        Expanded(
                          child: OutlinedButton(
                            onPressed: () async {
                              final picked = await showDatePicker(
                                context: context,
                                initialDate: DateTime.now(),
                                firstDate: DateTime.now(),
                                lastDate: DateTime.now().add(
                                  Duration(days: 365),
                                ),
                              );
                              if (picked != null) {
                                setModalState(() {
                                  deadline = picked;
                                });
                              }
                            },
                            child: Text("Pick Deadline"),
                          ),
                        ),
                        SizedBox(width: 12),
                        Expanded(
                          child: ElevatedButton(
                            onPressed: () {
                              if (nameController.text.isNotEmpty &&
                                  durationController.text.isNotEmpty) {
                                setState(() {
                                  tasks.add({
                                    "name": nameController.text,
                                    "duration": durationController.text,
                                    "deadline": deadline?.toIso8601String(),
                                    "completed": false,
                                  });
                                });
                                _saveTasks();
                                Navigator.pop(context);
                              }
                            },
                            child: Text("Add Task"),
                          ),
                        ),
                      ],
                    ),
                  ],
                ),
              ),
            );
          },
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

  void _toggleTaskCompletion(int index) {
    setState(() {
      tasks[index]["completed"] = !tasks[index]["completed"];
    });
    _saveTasks();
  }

  void _editTask(int index) {
    final task = tasks[index];
    final nameController = TextEditingController(text: task["name"]);
    final durationController = TextEditingController(text: task["duration"]);
    DateTime? deadline =
        task["deadline"] != null ? DateTime.parse(task["deadline"]) : null;

    showModalBottomSheet(
      isScrollControlled: true,
      context: context,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.vertical(top: Radius.circular(20)),
      ),
      builder: (BuildContext context) {
        return StatefulBuilder(
          builder: (context, setModalState) {
            return Container(
              padding: EdgeInsets.only(
                bottom: MediaQuery.of(context).viewInsets.bottom,
              ),
              child: Padding(
                padding: const EdgeInsets.all(24.0),
                child: Column(
                  mainAxisSize: MainAxisSize.min,
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      "Edit Task",
                      style: TextStyle(
                        fontSize: 24,
                        fontWeight: FontWeight.bold,
                      ),
                    ),
                    SizedBox(height: 20),

                    TextField(
                      controller: nameController,
                      decoration: InputDecoration(
                        labelText: "Task Name",
                        border: OutlineInputBorder(
                          borderRadius: BorderRadius.circular(12),
                        ),
                      ),
                    ),
                    SizedBox(height: 16),

                    TextField(
                      controller: durationController,
                      decoration: InputDecoration(
                        labelText: "Duration (minutes)",
                        border: OutlineInputBorder(
                          borderRadius: BorderRadius.circular(12),
                        ),
                      ),
                      keyboardType: TextInputType.number,
                    ),
                    SizedBox(height: 16),

                    Container(
                      width: double.infinity,
                      padding: EdgeInsets.all(16),
                      decoration: BoxDecoration(
                        border: Border.all(color: Colors.grey.shade300),
                        borderRadius: BorderRadius.circular(12),
                      ),
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text(
                            "Deadline",
                            style: TextStyle(color: Colors.grey[600]),
                          ),
                          SizedBox(height: 8),
                          Text(
                            deadline != null
                                ? "${deadline!.day}/${deadline!.month}/${deadline!.year}"
                                : "No deadline set",
                            style: TextStyle(fontSize: 16),
                          ),
                        ],
                      ),
                    ),
                    SizedBox(height: 20),

                    Row(
                      children: [
                        Expanded(
                          child: OutlinedButton(
                            onPressed: () async {
                              final picked = await showDatePicker(
                                context: context,
                                initialDate: deadline ?? DateTime.now(),
                                firstDate: DateTime.now(),
                                lastDate: DateTime.now().add(
                                  Duration(days: 365),
                                ),
                              );
                              if (picked != null) {
                                setModalState(() {
                                  deadline = picked;
                                });
                              }
                            },
                            child: Text("Change Deadline"),
                          ),
                        ),
                        SizedBox(width: 12),
                        Expanded(
                          child: ElevatedButton(
                            onPressed: () {
                              if (nameController.text.isNotEmpty &&
                                  durationController.text.isNotEmpty) {
                                setState(() {
                                  tasks[index] = {
                                    "name": nameController.text,
                                    "duration": durationController.text,
                                    "deadline": deadline?.toIso8601String(),
                                    "completed": task["completed"] ?? false,
                                  };
                                });
                                _saveTasks();
                                Navigator.pop(context);
                              }
                            },
                            child: Text("Update Task"),
                          ),
                        ),
                      ],
                    ),
                  ],
                ),
              ),
            );
          },
        );
      },
    );
  }

  Widget _buildTaskStats() {
    final completedTasks = tasks.where((t) => t['completed'] == true).length;
    final overdueTasks =
        tasks.where((t) {
          if (t['completed'] == true) return false;
          if (t['deadline'] == null) return false;
          return DateTime.parse(t['deadline']).isBefore(DateTime.now());
        }).length;
    final todayTasks =
        tasks.where((t) {
          if (t['deadline'] == null) return false;
          final deadline = DateTime.parse(t['deadline']);
          final today = DateTime.now();
          return deadline.year == today.year &&
              deadline.month == today.month &&
              deadline.day == today.day &&
              t['completed'] != true;
        }).length;

    return Card(
      margin: EdgeInsets.symmetric(horizontal: 16, vertical: 8),
      child: Padding(
        padding: EdgeInsets.all(16),
        child: Row(
          mainAxisAlignment: MainAxisAlignment.spaceAround,
          children: [
            _StatItem(
              icon: Icons.check_circle,
              label: "Completed",
              value: completedTasks.toString(),
              color: Colors.green,
            ),
            _StatItem(
              icon: Icons.today,
              label: "Due Today",
              value: todayTasks.toString(),
              color: Colors.blue,
            ),
            _StatItem(
              icon: Icons.warning,
              label: "Overdue",
              value: overdueTasks.toString(),
              color: Colors.red,
            ),
          ],
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Consumer<DeskCompanionService>(
      builder: (context, service, child) {
        return Scaffold(
          body: Column(
            children: [
              // Header with sync status
              Container(
                padding: EdgeInsets.all(16),
                child: Row(
                  children: [
                    Icon(Icons.task_alt, size: 24),
                    SizedBox(width: 12),
                    Expanded(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text(
                            "Tasks",
                            style: TextStyle(
                              fontSize: 18,
                              fontWeight: FontWeight.bold,
                            ),
                          ),
                          Text(
                            "${tasks.length} tasks • ${tasks.where((t) => t['completed'] != true).length} pending",
                            style: TextStyle(color: Colors.grey[600]),
                          ),
                        ],
                      ),
                    ),
                    Row(
                      children: [
                        Icon(
                          service.isConnected
                              ? Icons.sync
                              : Icons.sync_disabled,
                          color:
                              service.isConnected ? Colors.green : Colors.red,
                          size: 20,
                        ),
                        SizedBox(width: 4),
                        Text(
                          service.isConnected ? "Synced" : "No sync",
                          style: TextStyle(
                            color:
                                service.isConnected ? Colors.green : Colors.red,
                            fontSize: 12,
                          ),
                        ),
                      ],
                    ),
                  ],
                ),
              ),

              // Task Statistics
              if (tasks.isNotEmpty) _buildTaskStats(),

              // Tasks List
              Expanded(
                child:
                    tasks.isEmpty
                        ? Center(
                          child: Column(
                            mainAxisAlignment: MainAxisAlignment.center,
                            children: [
                              Icon(
                                Icons.assignment,
                                size: 64,
                                color: Colors.grey,
                              ),
                              SizedBox(height: 16),
                              Text(
                                "No tasks yet",
                                style: TextStyle(
                                  fontSize: 18,
                                  color: Colors.grey,
                                ),
                              ),
                              SizedBox(height: 8),
                              Text(
                                "Add your first task to get started",
                                style: TextStyle(color: Colors.grey[600]),
                              ),
                              SizedBox(height: 20),
                              ElevatedButton.icon(
                                onPressed: _addTask,
                                icon: Icon(Icons.add),
                                label: Text("Add Task"),
                              ),
                            ],
                          ),
                        )
                        : ListView.builder(
                          padding: EdgeInsets.symmetric(horizontal: 16),
                          itemCount: tasks.length,
                          itemBuilder: (context, index) {
                            final task = tasks[index];
                            final isCompleted = task["completed"] ?? false;
                            final deadline =
                                task["deadline"] != null
                                    ? DateTime.parse(task["deadline"])
                                    : null;
                            final isOverdue =
                                deadline != null &&
                                deadline.isBefore(DateTime.now()) &&
                                !isCompleted;
                            final isDueToday =
                                deadline != null &&
                                _isSameDay(deadline, DateTime.now()) &&
                                !isCompleted;

                            return Card(
                              margin: EdgeInsets.only(bottom: 8),
                              elevation: isOverdue ? 4 : 1,
                              color:
                                  isOverdue
                                      ? Colors.red.shade50
                                      : isDueToday
                                      ? Colors.blue.shade50
                                      : null,
                              child: ListTile(
                                leading: Checkbox(
                                  value: isCompleted,
                                  onChanged:
                                      (value) => _toggleTaskCompletion(index),
                                ),
                                title: Text(
                                  task["name"],
                                  style: TextStyle(
                                    decoration:
                                        isCompleted
                                            ? TextDecoration.lineThrough
                                            : null,
                                    color: isCompleted ? Colors.grey : null,
                                    fontWeight:
                                        isOverdue ? FontWeight.bold : null,
                                  ),
                                ),
                                subtitle: Column(
                                  crossAxisAlignment: CrossAxisAlignment.start,
                                  children: [
                                    SizedBox(height: 4),
                                    Row(
                                      children: [
                                        Icon(
                                          Icons.timer,
                                          size: 16,
                                          color: Colors.grey,
                                        ),
                                        SizedBox(width: 4),
                                        Text("${task["duration"]} minutes"),
                                        if (deadline != null) ...[
                                          SizedBox(width: 16),
                                          Icon(
                                            Icons.calendar_today,
                                            size: 16,
                                            color:
                                                isOverdue
                                                    ? Colors.red
                                                    : isDueToday
                                                    ? Colors.blue
                                                    : Colors.grey,
                                          ),
                                          SizedBox(width: 4),
                                          Text(
                                            _formatDeadline(deadline),
                                            style: TextStyle(
                                              color:
                                                  isOverdue
                                                      ? Colors.red
                                                      : isDueToday
                                                      ? Colors.blue
                                                      : Colors.grey,
                                              fontWeight:
                                                  (isOverdue || isDueToday)
                                                      ? FontWeight.bold
                                                      : null,
                                            ),
                                          ),
                                        ],
                                      ],
                                    ),
                                    SizedBox(height: 4),
                                    if (isOverdue)
                                      Container(
                                        padding: EdgeInsets.symmetric(
                                          horizontal: 8,
                                          vertical: 2,
                                        ),
                                        decoration: BoxDecoration(
                                          color: Colors.red,
                                          borderRadius: BorderRadius.circular(
                                            12,
                                          ),
                                        ),
                                        child: Text(
                                          "OVERDUE",
                                          style: TextStyle(
                                            color: Colors.white,
                                            fontWeight: FontWeight.bold,
                                            fontSize: 10,
                                          ),
                                        ),
                                      )
                                    else if (isDueToday)
                                      Container(
                                        padding: EdgeInsets.symmetric(
                                          horizontal: 8,
                                          vertical: 2,
                                        ),
                                        decoration: BoxDecoration(
                                          color: Colors.blue,
                                          borderRadius: BorderRadius.circular(
                                            12,
                                          ),
                                        ),
                                        child: Text(
                                          "DUE TODAY",
                                          style: TextStyle(
                                            color: Colors.white,
                                            fontWeight: FontWeight.bold,
                                            fontSize: 10,
                                          ),
                                        ),
                                      ),
                                  ],
                                ),
                                isThreeLine: true,
                                trailing: PopupMenuButton(
                                  itemBuilder:
                                      (context) => [
                                        PopupMenuItem(
                                          value: 'edit',
                                          child: Row(
                                            children: [
                                              Icon(
                                                Icons.edit,
                                                color: Colors.blue,
                                              ),
                                              SizedBox(width: 8),
                                              Text('Edit'),
                                            ],
                                          ),
                                        ),
                                        PopupMenuItem(
                                          value: 'delete',
                                          child: Row(
                                            children: [
                                              Icon(
                                                Icons.delete,
                                                color: Colors.red,
                                              ),
                                              SizedBox(width: 8),
                                              Text('Delete'),
                                            ],
                                          ),
                                        ),
                                      ],
                                  onSelected: (value) {
                                    if (value == 'edit') {
                                      _editTask(index);
                                    } else if (value == 'delete') {
                                      _showDeleteConfirmation(index);
                                    }
                                  },
                                ),
                              ),
                            );
                          },
                        ),
              ),
            ],
          ),
          floatingActionButton: FloatingActionButton(
            onPressed: _addTask,
            tooltip: 'Add Task',
            child: Icon(Icons.add),
          ),
        );
      },
    );
  }

  bool _isSameDay(DateTime date1, DateTime date2) {
    return date1.year == date2.year &&
        date1.month == date2.month &&
        date1.day == date2.day;
  }

  String _formatDeadline(DateTime deadline) {
    final now = DateTime.now();
    final today = DateTime(now.year, now.month, now.day);
    final tomorrow = today.add(Duration(days: 1));
    final deadlineDay = DateTime(deadline.year, deadline.month, deadline.day);

    if (deadlineDay == today) {
      return "Today";
    } else if (deadlineDay == tomorrow) {
      return "Tomorrow";
    } else if (deadlineDay.isBefore(today)) {
      final difference = today.difference(deadlineDay).inDays;
      return "$difference days ago";
    } else {
      return "${deadline.day}/${deadline.month}/${deadline.year}";
    }
  }

  void _showDeleteConfirmation(int index) {
    showDialog(
      context: context,
      builder: (BuildContext context) {
        return AlertDialog(
          title: Text("Delete Task"),
          content: Text(
            "Are you sure you want to delete '${tasks[index]["name"]}'?",
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.of(context).pop(),
              child: Text("Cancel"),
            ),
            TextButton(
              onPressed: () {
                _deleteTask(index);
                Navigator.of(context).pop();
              },
              style: TextButton.styleFrom(foregroundColor: Colors.red),
              child: Text("Delete"),
            ),
          ],
        );
      },
    );
  }
}

class _StatItem extends StatelessWidget {
  final IconData icon;
  final String label;
  final String value;
  final Color color;

  const _StatItem({
    required this.icon,
    required this.label,
    required this.value,
    required this.color,
  });

  @override
  Widget build(BuildContext context) {
    return Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        Container(
          padding: EdgeInsets.all(8),
          decoration: BoxDecoration(
            color: color.withAlpha(25),
            borderRadius: BorderRadius.circular(8),
          ),
          child: Icon(icon, color: color, size: 20),
        ),
        SizedBox(height: 4),
        Text(
          value,
          style: TextStyle(
            fontSize: 18,
            fontWeight: FontWeight.bold,
            color: color,
          ),
        ),
        Text(label, style: TextStyle(fontSize: 12, color: Colors.grey[600])),
      ],
    );
  }
}
