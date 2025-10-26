<!-- TROUBLESHOOTING WINWIDGETS APP -->

## Troubleshooting the WinWidgets Application

On occassion, you may need to determine why the WinWidgets application itself is not behaving as expected.

All logging for the main application is reported to the Windows Event Viewer. The Event Viewer application is installed on every Windows version and variant by default. The events can be found in the Windows Logs > Application section.

These are the following types of events you will find that are relevant to WinWidgets:
| Level       | Source                  | Event ID | Identifying Info (XML Path)                       |
|-------------|-------------------------|----------|---------------------------------------------------|
| Information | Windows Error Reporting | 1001     | EventData > Data Name=P1 > WidgetsDotNet.exe      |
| Error       | Application Error       | 1000     | EventData > Data Name=AppName > WidgetsDotNet.exe |

Within Event Viewer, it is highly recommended to create a filter to isolate the messages relevant to WinWidgets. The following is an example that captures the event types above, using an XML (XPath) filter:
```XML 
<QueryList>
  <Query Id="0" Path="Application">
    <Select Path="Application">*[EventData[Data[@Name='P1'] and (Data='WidgetsDotNet.exe')]]</Select>
    <Select Path="Application">*[EventData[Data[@Name='AppName'] and (Data='WidgetsDotNet.exe')]]</Select>
  </Query>
</QueryList>
```

## Reporting Bugs
If the resulting information shows an issue with WinWidgets itself, you can [report a bug](https://github.com/beyluta/WinWidgets/issues). Ensure you are on the latest stable release.

[![WinWidgets version](https://img.shields.io/badge/Version-1.5.1-green)](https://github.com/beyluta/WinWidgets/releases)


Please include as much information as possible, including details from the Event Viewer.

<!-- TODO: Add TROUBLESHOOTING CUSTOM WIDGETS section -->