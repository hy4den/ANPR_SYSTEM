import { useState } from 'react'
import LogsPage from './pages/LogsPage'
import WatchlistPage from './pages/WatchlistPage'

const TABS = [
  { id: 'logs',      label: 'Erisim Loglari',   icon: '▤' },
  { id: 'watchlist', label: 'Izin Listesi',      icon: '☰' },
]

function App() {
  const [tab, setTab] = useState('logs')

  return (
    <div className="layout">
      <aside className="sidebar">
        <div className="logo">
          <div className="logo-title">ANPR</div>
          <div className="logo-sub">Plaka Tanima Sistemi</div>
        </div>

        <nav>
          {TABS.map(t => (
            <button
              key={t.id}
              className={`nav-item ${tab === t.id ? 'active' : ''}`}
              onClick={() => setTab(t.id)}
            >
              <span className="nav-icon">{t.icon}</span>
              {t.label}
            </button>
          ))}
        </nav>

        <div className="sidebar-footer">
          <div>CENG318 - Grup 3</div>
          <div>Gazi Universitesi</div>
        </div>
      </aside>

      <main className="content">
        {tab === 'logs'      && <LogsPage />}
        {tab === 'watchlist' && <WatchlistPage />}
      </main>
    </div>
  )
}

export default App
