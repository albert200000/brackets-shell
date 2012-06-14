// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

// This file is shared by cefclient and cef_unittests so don't include using
// a qualified path.
#include "client_app.h"  // NOLINT(build/include)

#include <string>

#include "include/cef_process_message.h"
#include "include/cef_task.h"
#include "include/cef_v8.h"
#include "util.h"  // NOLINT(build/include)

namespace {

// Forward declarations.
void SetList(CefRefPtr<CefV8Value> source, CefRefPtr<CefListValue> target);
void SetList(CefRefPtr<CefListValue> source, CefRefPtr<CefV8Value> target);

// Transfer a V8 value to a List index.
void SetListValue(CefRefPtr<CefListValue> list, int index,
                  CefRefPtr<CefV8Value> value) {
  if (value->IsArray()) {
    CefRefPtr<CefListValue> new_list = CefListValue::Create();
    SetList(value, new_list);
    list->SetList(index, new_list);
  } else if (value->IsString()) {
    list->SetString(index, value->GetStringValue());
  } else if (value->IsBool()) {
    list->SetBool(index, value->GetBoolValue());
  } else if (value->IsInt()) {
    list->SetInt(index, value->GetIntValue());
  } else if (value->IsDouble()) {
    list->SetDouble(index, value->GetDoubleValue());
  }
}

// Transfer a V8 array to a List.
void SetList(CefRefPtr<CefV8Value> source, CefRefPtr<CefListValue> target) {
  ASSERT(source->IsArray());

  int arg_length = source->GetArrayLength();
  if (arg_length == 0)
    return;

  // Start with null types in all spaces.
  target->SetSize(arg_length);

  for (int i = 0; i < arg_length; ++i)
    SetListValue(target, i, source->GetValue(i));
}

CefRefPtr<CefV8Value> ListValueToV8Value(CefRefPtr<CefListValue> value, int index)
{
    CefRefPtr<CefV8Value> new_value;
    
    CefValueType type = value->GetType(index);
    switch (type) {
        case VTYPE_LIST: {
            CefRefPtr<CefListValue> list = value->GetList(index);
            new_value = CefV8Value::CreateArray(list->GetSize());
            SetList(list, new_value);
        } break;
        case VTYPE_BOOL:
            new_value = CefV8Value::CreateBool(value->GetBool(index));
            break;
        case VTYPE_DOUBLE:
            new_value = CefV8Value::CreateDouble(value->GetDouble(index));
            break;
        case VTYPE_INT:
            new_value = CefV8Value::CreateInt(value->GetInt(index));
            break;
        case VTYPE_STRING:
            new_value = CefV8Value::CreateString(value->GetString(index));
            break;
        default:
            new_value = CefV8Value::CreateNull();
            break;
    }
    
    return new_value;
}
    
// Transfer a List value to a V8 array index.
void SetListValue(CefRefPtr<CefV8Value> list, int index,
                  CefRefPtr<CefListValue> value) {
  CefRefPtr<CefV8Value> new_value;

  CefValueType type = value->GetType(index);
  switch (type) {
    case VTYPE_LIST: {
      CefRefPtr<CefListValue> list = value->GetList(index);
      new_value = CefV8Value::CreateArray(list->GetSize());
      SetList(list, new_value);
      } break;
    case VTYPE_BOOL:
      new_value = CefV8Value::CreateBool(value->GetBool(index));
      break;
    case VTYPE_DOUBLE:
      new_value = CefV8Value::CreateDouble(value->GetDouble(index));
      break;
    case VTYPE_INT:
      new_value = CefV8Value::CreateInt(value->GetInt(index));
      break;
    case VTYPE_STRING:
      new_value = CefV8Value::CreateString(value->GetString(index));
      break;
    default:
      break;
  }

  if (new_value.get()) {
    list->SetValue(index, new_value);
  } else {
    list->SetValue(index, CefV8Value::CreateNull());
  }
}

// Transfer a List to a V8 array.
void SetList(CefRefPtr<CefListValue> source, CefRefPtr<CefV8Value> target) {
  ASSERT(target->IsArray());

  int arg_length = source->GetSize();
  if (arg_length == 0)
    return;

  for (int i = 0; i < arg_length; ++i)
    SetListValue(target, i, source);
}


// Handles the native implementation for the appshell extension.
class AppShellExtensionHandler : public CefV8Handler {
 public:
  explicit AppShellExtensionHandler(CefRefPtr<ClientApp> client_app)
    : client_app_(client_app)
    , messageId(0) {
  }

  virtual bool Execute(const CefString& name,
                       CefRefPtr<CefV8Value> object,
                       const CefV8ValueList& arguments,
                       CefRefPtr<CefV8Value>& retval,
                       CefString& exception) {
      
      // The only message that is handled here is getElapsedMilliseconds(). All other
      // messages are passed to the browser process.
      if (name == "GetElapsedMilliseconds") {
          retval = CefV8Value::CreateDouble(client_app_->GetElapsedMilliseconds());
      } else {
          // Pass all messages to the browser process. Look in appshell_extensions.cpp for implementation.
          CefRefPtr<CefBrowser> browser = 
                CefV8Context::GetCurrentContext()->GetBrowser();
          ASSERT(browser.get());
          CefRefPtr<CefProcessMessage> message = 
                CefProcessMessage::Create(name);
          CefRefPtr<CefListValue> messageArgs = message->GetArgumentList();
          
          // The first argument must be a callback function
          if (arguments.size() > 0 && !arguments[0]->IsFunction()) {
              std::string functionName = name;
              fprintf(stderr, "Function called without callback param: %s\n", functionName.c_str());
              return false;
          } 
          
          if (arguments.size() > 0) {
              // The first argument is the message id
              client_app_->AddCallback(messageId, CefV8Context::GetCurrentContext(), arguments[0]);
              SetListValue(messageArgs, 0, CefV8Value::CreateInt(messageId));
          }
          
          // Pass the rest of the arguments
          for (unsigned int i = 1; i < arguments.size(); i++)
              SetListValue(messageArgs, i, arguments[i]);
          browser->SendProcessMessage(PID_BROWSER, message);
          
          messageId++;
      }
	
	return true;
  }

 private:
    CefRefPtr<ClientApp> client_app_;
    int32 messageId;
	
    IMPLEMENT_REFCOUNTING(AppShellExtensionHandler);
};

}  // namespace


ClientApp::ClientApp()
    : proxy_type_(PROXY_TYPE_DIRECT) {
  CreateRenderDelegates(render_delegates_);
}

void ClientApp::GetProxyForUrl(const CefString& url,
                               CefProxyInfo& proxy_info) {
  proxy_info.proxyType = proxy_type_;
  if (!proxy_config_.empty())
    CefString(&proxy_info.proxyList) = proxy_config_;
}

void ClientApp::OnWebKitInitialized() {
  // Register the appshell extension.
  std::string extension_code = GetExtensionJSSource();

  CefRegisterExtension("appshell", extension_code,
      new AppShellExtensionHandler(this));

  // Execute delegate callbacks.
  RenderDelegateSet::iterator it = render_delegates_.begin();
  for (; it != render_delegates_.end(); ++it)
    (*it)->OnWebKitInitialized(this);
}

void ClientApp::OnContextCreated(CefRefPtr<CefBrowser> browser,
                               CefRefPtr<CefFrame> frame,
                               CefRefPtr<CefV8Context> context) {
  // Execute delegate callbacks.
  RenderDelegateSet::iterator it = render_delegates_.begin();
  for (; it != render_delegates_.end(); ++it)
    (*it)->OnContextCreated(this, browser, frame, context);
}

void ClientApp::OnContextReleased(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefRefPtr<CefV8Context> context) {
  // Execute delegate callbacks.
  RenderDelegateSet::iterator it = render_delegates_.begin();
  for (; it != render_delegates_.end(); ++it)
    (*it)->OnContextReleased(this, browser, frame, context);
}

bool ClientApp::OnProcessMessageRecieved(
        CefRefPtr<CefBrowser> browser,
        CefProcessId source_process,
        CefRefPtr<CefProcessMessage> message) {
    ASSERT(source_process == PID_BROWSER);

    bool handled = false;

    // Execute delegate callbacks.
    RenderDelegateSet::iterator it = render_delegates_.begin();
    for (; it != render_delegates_.end() && !handled; ++it) {
        handled = (*it)->OnProcessMessageRecieved(this, browser, source_process, message);
    }

    if (!handled) {
        if (message->GetName() == "invokeCallback") {
            CefRefPtr<CefListValue> messageArgs = message->GetArgumentList();
            int32 callbackId = messageArgs->GetInt(0);
                    
            CefRefPtr<CefV8Context> context = callback_map_[callbackId].first;
            CefRefPtr<CefV8Value> callbackFunction = callback_map_[callbackId].second;
            CefV8ValueList arguments;
            context->Enter();
               
            for (size_t i = 1; i < messageArgs->GetSize(); i++) {
                arguments.push_back(ListValueToV8Value(messageArgs, i));
            }
            
            callbackFunction->ExecuteFunction(NULL, arguments);
            context->Exit();
            
            callback_map_.erase(callbackId);
        }
    }
    
    return handled;
}