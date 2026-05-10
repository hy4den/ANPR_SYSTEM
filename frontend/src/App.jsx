import { useEffect, useState } from 'react'
import LogsPage from './pages/LogsPage'
import WatchlistPage from './pages/WatchlistPage'
import { enableMobileNotifications, setupForegroundNotificationHandler } from './pushNotifications'

const TABS = [
  { id: 'logs',      label: 'Erisim Loglari',   icon: '▤' },
  { id: 'watchlist', label: 'Izin Listesi',      icon: '☰' },
]

function App() {
  const [tab, setTab] = useState('logs')
  const [pushStatus, setPushStatus] = useState('')
  const [pushBusy, setPushBusy] = useState(false)

  useEffect(() => {
    setupForegroundNotificationHandler((payload) => {
      const body = payload?.notification?.body || 'Yeni mobil bildirim'
      setPushStatus(`Anlik bildirim alindi: ${body}`)
    }).catch(() => {})
  }, [])

  const handleEnablePush = async () => {
    setPushBusy(true)
    setPushStatus('')
    try {
      await enableMobileNotifications()
      setPushStatus('Mobil bildirimler aktif edildi')
    } catch (e) {
      setPushStatus(e.message)
    } finally {
      setPushBusy(false)
    }
  }

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
          <button
            className="btn btn-primary"
            onClick={handleEnablePush}
            disabled={pushBusy}
            style={{ width: '100%', marginBottom: 8 }}
          >
            {pushBusy ? 'Ayaraniyor...' : 'Mobil Bildirimleri Ac'}
          </button>
          {pushStatus && <div style={{ fontSize: 12, marginBottom: 8 }}>{pushStatus}</div>}
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
