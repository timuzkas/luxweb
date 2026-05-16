#include <luxweb/style.hpp>

#include <fmt/core.h>

namespace lux {

std::string generate_js() {
  return R"JS((function (global) {
  "use strict";

  var activeEffect = null;
  var stores = Object.create(null);
  var actions = Object.create(null);

  function clone(value) {
    if (value === undefined || value === null || typeof value !== "object") {
      return value;
    }
    return JSON.parse(JSON.stringify(value));
  }

  function pathParts(path) {
    if (typeof path !== "string" || !/^[A-Za-z_$][\w$]*(\.[A-Za-z_$][\w$]*)*$/.test(path)) {
      return [];
    }
    return path.split(".");
  }

  function getPath(source, path) {
    var parts = pathParts(path);
    var value = source;
    for (var i = 0; i < parts.length; i += 1) {
      if (value == null) {
        return undefined;
      }
      value = value[parts[i]];
    }
    return value;
  }

  function setPath(source, path, value) {
    var parts = pathParts(path);
    if (!parts.length) {
      return false;
    }
    var cursor = source;
    for (var i = 0; i < parts.length - 1; i += 1) {
      var key = parts[i];
      if (cursor[key] == null || typeof cursor[key] !== "object") {
        cursor[key] = {};
      }
      cursor = cursor[key];
    }
    cursor[parts[parts.length - 1]] = value;
    return true;
  }

  function subscribeTo(set, listener) {
    set.add(listener);
    return function () {
      set.delete(listener);
    };
  }

  function signal(initial) {
    var value = initial;
    var listeners = new Set();
    return {
      get: function () {
        if (activeEffect) {
          listeners.add(activeEffect);
        }
        return value;
      },
      set: function (next) {
        value = next;
        Array.from(listeners).forEach(function (listener) {
          listener();
        });
      },
      update: function (fn) {
        this.set(fn(value));
      },
      subscribe: function (listener) {
        listener(value);
        return subscribeTo(listeners, function () {
          listener(value);
        });
      }
    };
  }

  function effect(fn) {
    function run() {
      var previous = activeEffect;
      activeEffect = run;
      try {
        fn();
      } finally {
        activeEffect = previous;
      }
    }
    run();
    return run;
  }

  function readPersisted(name, fallback, options) {
    if (!options || !options.persist || !global.localStorage) {
      return clone(fallback);
    }
    var key = typeof options.persist === "string" ? options.persist : "lux:" + name;
    try {
      var raw = global.localStorage.getItem(key);
      if (!raw) {
        return clone(fallback);
      }
      var parsed = JSON.parse(raw);
      if (fallback && typeof fallback === "object" && parsed && typeof parsed === "object" && !Array.isArray(parsed)) {
        return Object.assign(clone(fallback), parsed);
      }
      return parsed;
    } catch (_) {
      global.localStorage.removeItem(key);
      return clone(fallback);
    }
  }

  function writePersisted(name, value, options) {
    if (!options || !options.persist || !global.localStorage) {
      return;
    }
    var key = typeof options.persist === "string" ? options.persist : "lux:" + name;
    try {
      global.localStorage.setItem(key, JSON.stringify(value));
    } catch (_) {
      global.localStorage.removeItem(key);
    }
  }

  function store(name, initialState, options) {
    if (stores[name]) {
      return stores[name];
    }
    var state = readPersisted(name, initialState || {}, options || {});
    var listeners = new Set();

    function publish() {
      writePersisted(name, state, options || {});
      Array.from(listeners).forEach(function (listener) {
        listener(clone(state));
      });
    }

    var api = {
      name: name,
      get: function (path) {
        if (activeEffect) {
          listeners.add(activeEffect);
        }
        return path ? getPath(state, path) : clone(state);
      },
      set: function (path, value) {
        if (arguments.length === 1) {
          state = clone(path);
        } else if (!setPath(state, path, value)) {
          return;
        }
        publish();
      },
      patch: function (partial) {
        if (partial && typeof partial === "object") {
          Object.keys(partial).forEach(function (key) {
            state[key] = partial[key];
          });
          publish();
        }
      },
      subscribe: function (listener) {
        listeners.add(listener);
        listener(clone(state));
        return function () {
          listeners.delete(listener);
        };
      }
    };
    stores[name] = api;
    return api;
  }

  function tokenValue(token) {
    token = token.trim();
    if (token === "true") return true;
    if (token === "false") return false;
    if (token === "null") return null;
    if (/^-?\d+(\.\d+)?$/.test(token)) return Number(token);
    if ((token[0] === "\"" && token[token.length - 1] === "\"") || (token[0] === "'" && token[token.length - 1] === "'")) {
      return token.slice(1, -1);
    }
    if (/^[A-Za-z_$][\w$]*(\.[A-Za-z_$][\w$]*)*$/.test(token)) {
      var parts = token.split(".");
      var named = stores[parts.shift()];
      return named ? named.get(parts.join(".")) : undefined;
    }
    return undefined;
  }

  function compare(left, operator, right) {
    switch (operator) {
      case "==": return left == right;
      case "!=": return left != right;
      case "===": return left === right;
      case "!==": return left !== right;
      case ">": return left > right;
      case ">=": return left >= right;
      case "<": return left < right;
      case "<=": return left <= right;
      default: return false;
    }
  }

  function evaluate(expr) {
    expr = String(expr || "").trim();
    if (expr[0] === "!") {
      return !evaluate(expr.slice(1));
    }
    var match = expr.match(/^(.+?)\s*(===|!==|==|!=|>=|<=|>|<)\s*(.+)$/);
    if (match) {
      return compare(tokenValue(match[1]), match[2], tokenValue(match[3]));
    }
    return tokenValue(expr);
  }

  function modelValue(element) {
    if (element.type === "checkbox") {
      return Boolean(element.checked);
    }
    if (element.type === "number" || element.type === "range") {
      return element.value === "" ? null : Number(element.value);
    }
    return element.value;
  }

  function setInput(element, value) {
    if (element.type === "checkbox") {
      element.checked = Boolean(value);
    } else if (element.value !== String(value == null ? "" : value)) {
      element.value = value == null ? "" : value;
    }
  }

  function bindModel(element, expr) {
    var parts = String(expr || "").split(".");
    var named = stores[parts.shift()];
    var path = parts.join(".");
    if (!named || !path) {
      return;
    }
    if (!element.__luxModelBound) {
      element.addEventListener("input", function () {
        named.set(path, modelValue(element));
      });
      element.addEventListener("change", function () {
        named.set(path, modelValue(element));
      });
      element.__luxModelBound = true;
    }
    effect(function () {
      setInput(element, named.get(path));
    });
  }

  function mount(root) {
    root = root || document;
    function all(selector) {
      var found = Array.from(root.querySelectorAll(selector));
      if (root.nodeType === 1 && root.matches(selector)) {
        found.unshift(root);
      }
      return found;
    }
    all("[data-lux-text]").forEach(function (element) {
      effect(function () {
        var value = evaluate(element.getAttribute("data-lux-text"));
        element.textContent = value == null ? "" : String(value);
      });
    });
    all("[data-lux-html]").forEach(function (element) {
      effect(function () {
        var value = evaluate(element.getAttribute("data-lux-html"));
        element.innerHTML = value == null ? "" : String(value);
      });
    });
    all("[data-lux-show]").forEach(function (element) {
      effect(function () {
        element.hidden = !evaluate(element.getAttribute("data-lux-show"));
      });
    });
    all("[data-lux-model]").forEach(function (element) {
      bindModel(element, element.getAttribute("data-lux-model"));
    });
    all("*").forEach(function (element) {
      Array.from(element.attributes).forEach(function (attr) {
        if (attr.name.indexOf("data-lux-class:") === 0) {
          var className = attr.name.slice("data-lux-class:".length);
          effect(function () {
            element.classList.toggle(className, Boolean(evaluate(attr.value)));
          });
        }
        if (attr.name.indexOf("data-lux-on:") === 0 && !element.__luxEvents) {
          element.__luxEvents = Object.create(null);
        }
        if (attr.name.indexOf("data-lux-on:") === 0) {
          var eventName = attr.name.slice("data-lux-on:".length);
          if (!element.__luxEvents[eventName]) {
            element.addEventListener(eventName, function (event) {
              var actionName = element.getAttribute("data-lux-on:" + event.type);
              if (actions[actionName]) {
                actions[actionName](event, element);
              }
            });
            element.__luxEvents[eventName] = true;
          }
        }
      });
    });
    return root;
  }

  function action(name, fn) {
    actions[name] = fn;
    return fn;
  }

  var lux = global.lux || {};
  lux.signal = signal;
  lux.effect = effect;
  lux.store = store;
  lux.action = action;
  lux.mount = mount;
  lux.evaluate = evaluate;
  lux.stores = stores;
  global.lux = lux;

  if (typeof document !== "undefined") {
    if (document.readyState === "complete") {
      mount(document);
    } else {
      document.addEventListener("DOMContentLoaded", function () {
        mount(document);
      });
    }
  }
})(window);
)JS";
}

std::string generate_css(const StyleTheme& t) {
  return fmt::format(R"CSS(:root {{
  --lux-brand: {0};
  --lux-accent: {1};
  --lux-surface: {2};
  --lux-bg: {3};
  --lux-text: {4};
  --lux-muted: {5};
  --lux-border: {6};
  --lux-radius: {7};
  --lux-space: {8}px;
  --lux-font: {9};
}}
* {{ box-sizing: border-box; }}
body {{
  margin: 0;
  background: var(--lux-bg);
  color: var(--lux-text);
  font-family: var(--lux-font);
  line-height: 1.5;
}}
a {{ color: var(--lux-brand); text-decoration: none; }}
a:hover {{ text-decoration: underline; }}
.lux-shell {{ max-width: 1040px; margin: 0 auto; padding: 24px; }}
.lux-nav {{ display: flex; align-items: center; justify-content: space-between; gap: 16px; padding: 16px 0; }}
.lux-nav__links {{ display: flex; gap: 12px; align-items: center; flex-wrap: wrap; }}
.lux-brand {{ font-size: 20px; font-weight: 750; color: var(--lux-text); }}
.lux-panel {{
  background: var(--lux-surface);
  border: 1px solid var(--lux-border);
  border-radius: var(--lux-radius);
  padding: 24px;
}}
.lux-grid {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(240px, 1fr)); gap: 16px; }}
.lux-card {{ background: var(--lux-surface); border: 1px solid var(--lux-border); border-radius: var(--lux-radius); padding: 18px; }}
.lux-button, button {{
  appearance: none;
  border: 0;
  border-radius: var(--lux-radius);
  background: var(--lux-brand);
  color: white;
  cursor: pointer;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  font: inherit;
  font-weight: 650;
  min-height: 40px;
  padding: 8px 14px;
}}
.lux-button.secondary {{ background: #eef2f7; color: var(--lux-text); }}
input {{
  width: 100%;
  border: 1px solid var(--lux-border);
  border-radius: var(--lux-radius);
  font: inherit;
  min-height: 42px;
  padding: 8px 10px;
}}
label {{ display: grid; gap: 6px; font-weight: 650; margin-bottom: 12px; }}
.lux-form {{ max-width: 440px; display: grid; gap: 8px; }}
.lux-error {{ color: #b42318; background: #fff1f0; border: 1px solid #fecdca; border-radius: var(--lux-radius); padding: 10px; }}
.lux-muted {{ color: var(--lux-muted); }}
pre {{ overflow: auto; background: #111827; color: #f9fafb; border-radius: var(--lux-radius); padding: 16px; }}
)CSS",
                     t.brand, t.accent, t.surface, t.background, t.text, t.muted, t.border, t.radius, t.spacing,
                     t.font_family);
}

}  // namespace lux
