# To Do Widget

A compact Microsoft To Do widget with full task editing via Microsoft Graph.

## Features

- **Microsoft Account Login**  
    Secure OAuth2 login using PKCE flow. Auth code is handled via a redirect page and stored in localStorage.

- **Task List Display**  
    Shows your default "Tasks" list from Microsoft To Do. List name is displayed in the header.

- **Samsung Reminders Sync**  
    If you are a Samsung mobile user, you can sync your Samsung Reminders with Microsoft To Do. These reminders will also appear in the widget alongside your other tasks.

- **Add, Edit, and Delete Tasks**  
    - Add new tasks with title, due date, reminder, and importance.
    - Edit task title and notes inline.
    - Delete tasks via the task menu.

- **Mark Tasks Complete**  
    Click the checkbox to mark a task as completed or not started.

- **Set Due Dates and Reminders**  
    - Set due date and reminder for each task using popup editors.
    - Dates use your local timezone.

- **Task Importance**  
    Toggle importance (starred) for each task.

- **Task Notes**  
    Expand/collapse notes for each task. Edit notes inline.

- **Task Categories**  
    Display categories as chips if present.

- **Drag and Drop Reordering**  
    Drag tasks to reorder them visually.

- **Auto Refresh**  
    Tasks auto-refresh every 60 seconds.

- **Sign Out**  
    Sign out and clear all local data with confirmation popup.

- **Responsive UI**  
    - Compact design (280x320px widget).
    - Styled with dark theme and smooth animations.

## Usage

1. Drag & Drop `microsoft-todo_widget.html` in WinWidget.
2. Click "Sign in" to authenticate and start managing your tasks.