(function () {
  const user = lux.store('user', { authenticated: false, user: null, ready: false }, { persist: 'luxweb:user' });
  const dashboard = lux.store('dashboard', { count: 0, note: 'Reactive dashboard' }, { persist: true });

  function snapshot() {
    return user.get();
  }

  function set(next) {
    if (next.authenticated && next.user) {
      user.set({ authenticated: true, user: next.user, ready: true });
    } else {
      localStorage.removeItem('luxweb:user');
      user.set({ authenticated: false, user: null, ready: true });
    }
  }

  async function refresh() {
    const response = await fetch('/api/me', { credentials: 'same-origin', headers: { Accept: 'application/json' } });
    set(await response.json());
    return snapshot();
  }

  function subscribe(listener) {
    return user.subscribe(listener);
  }

  window.lux = window.lux || {};
  window.lux.user = { get: snapshot, set, refresh, subscribe };
  window.lux.action('dashboard.increment', () => {
    dashboard.set('count', dashboard.get('count') + 1);
  });
  window.lux.action('dashboard.decrement', () => {
    dashboard.set('count', dashboard.get('count') - 1);
  });
  window.lux.action('dashboard.reset', () => {
    dashboard.set('count', 0);
  });

  user.subscribe((state) => {
    window.dispatchEvent(new CustomEvent('lux:user', { detail: state }));
  });

  refresh().catch(() => {
    localStorage.removeItem('luxweb:user');
    user.set({ authenticated: false, user: null, ready: true });
  });
})();
