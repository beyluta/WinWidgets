var version;

function setVersion(version) {
  document.querySelector("#version").innerHTML = `v${version}`;
}

function filter(s) {
  const children = document.getElementById("widgets").children;
  for (let element of children) {
    if (s.length > 0) {
      if (
        element.getAttribute("name").toLowerCase().includes(s.toLowerCase())
      ) {
        element.style.display = "flex";
      } else {
        element.style.display = "none";
      }
    } else {
      element.style.display = "flex";
    }
  }
}

function changeText(id, text) {
  document.getElementById(id).innerHTML = text;
}

function changeTab(element) {
  const dashboard = document.getElementById("dashboard");
  const settings = document.getElementById("settings");
  const development = document.getElementById("development");
  dashboard.style.fill = "var(--gray)";
  settings.style.fill = "var(--gray)";
  development.style.fill = "var(--gray)";
  element.style.fill = "var(--black)";

  switch (element.getAttribute("tab")) {
    case "dashboard":
      document.getElementById("widgets").style.display = "flex";
      document.getElementById("options").style.display = "none";
      document.getElementById("searchcontainer").style.display = "flex";
      document.getElementById("nextwindowtitle").style.display = "block";
      document.getElementById("windowtitle").innerHTML = "Installed Widgets";
      document.getElementById("development-container").style.display = "none";
      break;

    case "development":
      document.getElementById("widgets").style.display = "none";
      document.getElementById("options").style.display = "none";
      document.getElementById("searchcontainer").style.display = "none";
      document.getElementById("nextwindowtitle").style.display = "none";
      document.getElementById("windowtitle").innerHTML = "Development";
      document.getElementById("development-container").style.display = "flex";
      break;

    case "settings":
      document.getElementById("widgets").style.display = "none";
      document.getElementById("options").style.display = "flex";
      document.getElementById("searchcontainer").style.display = "none";
      document.getElementById("nextwindowtitle").style.display = "none";
      document.getElementById("windowtitle").innerHTML = "Settings";
      document.getElementById("development-container").style.display = "none";
      break;
  }
}

function setSwitchState(element) {
  if (element.classList.contains("switchon")) {
    element.classList.remove("switchon");
  } else {
    element.classList.add("switchon");
  }
}

function setOptionState(element) {
  setSwitchState(element);
  CefSharp.PostMessage(element.getAttribute("setting"));
}

window.onload = () => {
  const searchWidget = document.getElementById("searchwidget");

  searchWidget.onchange = function () {
    if (searchWidget.value.length > 0) {
      changeText("windowtitle", "Browse Widgets");
      changeText("nextwindowtitle", "Installed Widgets");
    } else {
      changeText("windowtitle", "Installed Widgets");
      changeText("nextwindowtitle", "Browse Widgets");
    }
    filter(searchWidget.value);
  };
};
