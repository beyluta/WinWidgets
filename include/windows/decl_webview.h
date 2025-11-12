// ======================= Purpose ==========================
//
// Header file cotaining all required function definitions
// and handler objects that WebView2 requires to work.
//
// In a C++ codebase, you would naturally have just a single
// object which you can instantiate and use its functions and
// properties. Since this isn't the case in C, we need to
// create a virtual table of function pointers before we can
// use the handler objects.
//
// ==========================================================
#ifndef DECL_WEBVIEW_H
#define DECL_WEBVIEW_H

#include <WebView2.h>
#include <stdint.h>

typedef enum : uint8_t
{
        FUNC_STATUS_OK,
        FUNC_STATUS_USR_ERR,
        FUNC_STATUS_WEBVIEW_ERR,
        FUNC_STATUS_MEM_ERR,
        FUNC_STATUS_ERR
} func_status_t;

/**
 * @brief Forward declaration of functions that are needed before creating a
 * WebView event handler.
 */
ULONG
HandlerAddRef(IUnknown *);

ULONG
HandlerRelease(IUnknown *);

HRESULT
HandlerQueryInterface(IUnknown *this, const IID *, void **ppvObject);

func_status_t
EnvironmentCompletedHandlerInvoke(const IUnknown *const this,
                                  const HRESULT errorCode,
                                  const ICoreWebView2Environment *const arg);

func_status_t
ControllerCompletedHandlerInvoke(const IUnknown *const this,
                                 const HRESULT errorCode,
                                 const ICoreWebView2Controller *const arg);

func_status_t
WebView2ContextMenuRequestEventHandlerInvoke(
        ICoreWebView2ContextMenuRequestedEventHandler *const this,
        ICoreWebView2 *const sender,
        ICoreWebView2ContextMenuRequestedEventArgs *const args);

func_status_t
ManagerWebMessageReceivedEventHandlerInvoke(
        ICoreWebView2WebMessageReceivedEventHandler *const handler,
        ICoreWebView2 *const webview,
        ICoreWebView2WebMessageReceivedEventArgs *const args);

func_status_t
WidgetWebMessageReceivedEventHandlerInvoke(
        ICoreWebView2WebMessageReceivedEventHandler *const handler,
        ICoreWebView2 *const webview,
        ICoreWebView2WebMessageReceivedEventArgs *const args);

func_status_t
OnCloseContextItemMenuSelected(ICoreWebView2ContextMenuItem *const sender,
                               IUnknown *const args);

func_status_t
OnMoveContextItemMenuSelected(ICoreWebView2ContextMenuItem *const sender,
                              IUnknown *const args);

func_status_t
OnTopMostContextItemMenuSelected(ICoreWebView2ContextMenuItem *const sender,
                                 IUnknown *const args);

/**
 * @brief Used to the create the virtual tables and handler objects for various
 * WebView2 Handlers.
 *
 * @param func User defined function name that will be invoked. Make sure there
 * is a forward declaration for this function above.
 * @param type Type of the handler
 * @param name User defined name of the handler
 */
#define CREATE_WEBVIEW_HANDLER(func, type, name)                               \
        type##Vtbl name##Vtbl = {                                              \
                .AddRef = (void *)HandlerAddRef,                               \
                .Release = (void *)HandlerRelease,                             \
                .QueryInterface = (void *)HandlerQueryInterface,               \
                .Invoke = (void *)func};                                       \
        type name = {.lpVtbl = &name##Vtbl};

CREATE_WEBVIEW_HANDLER(ManagerWebMessageReceivedEventHandlerInvoke,
                       ICoreWebView2WebMessageReceivedEventHandler,
                       managerMessageReceivedEventHandler);

CREATE_WEBVIEW_HANDLER(WidgetWebMessageReceivedEventHandlerInvoke,
                       ICoreWebView2WebMessageReceivedEventHandler,
                       widgetMessageReceivedEventHandler);

CREATE_WEBVIEW_HANDLER(WebView2ContextMenuRequestEventHandlerInvoke,
                       ICoreWebView2ContextMenuRequestedEventHandler,
                       contextMenuEventHandler);

CREATE_WEBVIEW_HANDLER(OnCloseContextItemMenuSelected,
                       ICoreWebView2CustomItemSelectedEventHandler,
                       closeMenuSelectedHandler);

CREATE_WEBVIEW_HANDLER(OnMoveContextItemMenuSelected,
                       ICoreWebView2CustomItemSelectedEventHandler,
                       moveMenuSelectedHandler);

CREATE_WEBVIEW_HANDLER(OnTopMostContextItemMenuSelected,
                       ICoreWebView2CustomItemSelectedEventHandler,
                       topMostMenuSelectedHandler);

CREATE_WEBVIEW_HANDLER(
        ControllerCompletedHandlerInvoke,
        ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,
        controllerCompletedHandler);

CREATE_WEBVIEW_HANDLER(
        EnvironmentCompletedHandlerInvoke,
        ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,
        environmentCompletedHandler);

#endif
