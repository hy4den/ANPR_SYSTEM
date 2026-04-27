import { useState, useEffect, useCallback } from 'react'
import { getLogs, imageUrl } from '../api'

const EMPTY_FILTERS = { plate: '', from: '', to: '', status: '' }

function LogsPage() {
  const [logs, setLogs]           = useState([])
  const [loading, setLoading]     = useState(false)
  const [error, setError]         = useState(null)
  const [filters, setFilters]     = useState(EMPTY_FILTERS)
  const [autoRefresh, setAuto]    = useState(true)

  const fetchLogs = useCallback(async () => {
    setLoading(true)
    setError(null)
    try {
      const params = {}
      if (filters.plate)  params.plate  = filters.plate
      if (filters.from)   params.from   = filters.from.replace('T', ' ')
      if (filters.to)     params.to     = filters.to.replace('T', ' ')
      if (filters.status !== '') params.status = filters.status
      setLogs(await getLogs(params))
    } catch (e) {
      setError(e.message)
    } finally {
      setLoading(false)
    }
  }, [filters])

  // Initial load + filter change
  useEffect(() => { fetchLogs() }, [fetchLogs])

  // Auto-refresh every 10 seconds
  useEffect(() => {
    if (!autoRefresh) return
    const id = setInterval(fetchLogs, 10_000)
    return () => clearInterval(id)
  }, [autoRefresh, fetchLogs])

  const handleFilter = e => {
    const { name, value } = e.target
    setFilters(f => ({ ...f, [name]: value }))
  }

  const handleReset = () => setFilters(EMPTY_FILTERS)

  return (
    <div>
      <div className="page-header">
        <h2>Erisim Loglari</h2>
        <label className="toggle">
          <input
            type="checkbox"
            checked={autoRefresh}
            onChange={e => setAuto(e.target.checked)}
          />
          <span>Otomatik yenile (10s)</span>
        </label>
      </div>

      {/* Filter bar */}
      <div className="filters">
        <input
          className="input"
          style={{ minWidth: 160 }}
          placeholder="Plaka ara..."
          name="plate"
          value={filters.plate}
          onChange={handleFilter}
        />
        <input
          className="input"
          type="datetime-local"
          name="from"
          value={filters.from}
          onChange={handleFilter}
          title="Baslangic tarihi"
        />
        <input
          className="input"
          type="datetime-local"
          name="to"
          value={filters.to}
          onChange={handleFilter}
          title="Bitis tarihi"
        />
        <select className="input" name="status" value={filters.status} onChange={handleFilter}>
          <option value="">Tum Durumlar</option>
          <option value="1">Gecis Verildi</option>
          <option value="0">Reddedildi</option>
        </select>
        <button className="btn btn-primary" onClick={fetchLogs}>Ara</button>
        <button
          className="btn"
          style={{ background: 'var(--surface2)', color: 'var(--muted)' }}
          onClick={handleReset}
        >
          Temizle
        </button>
      </div>

      {error && <div className="alert alert-error">{error}</div>}

      <div className="table-wrap">
        <table className="table">
          <thead>
            <tr>
              <th>#</th>
              <th>Zaman</th>
              <th>Plaka</th>
              <th>OCR Guven</th>
              <th>Durum</th>
              <th>Goruntu</th>
            </tr>
          </thead>
          <tbody>
            {loading && logs.length === 0 ? (
              <tr><td colSpan={6} className="td-center">Yukleniyor...</td></tr>
            ) : logs.length === 0 ? (
              <tr><td colSpan={6} className="td-center">Kayit bulunamadi</td></tr>
            ) : logs.map(l => (
              <tr key={l.log_id}>
                <td className="td-mono">{l.log_id}</td>
                <td className="td-mono">{l.timestamp}</td>
                <td>
                  <span className="plate-chip">{l.license_plate || '—'}</span>
                </td>
                <td>
                  <span className={l.confidence < 70 ? 'td-warn' : ''}>
                    {l.confidence != null ? `${l.confidence.toFixed(1)}%` : '—'}
                    {l.confidence > 0 && l.confidence < 70 && (
                      <span title="Manuel inceleme gerekebilir"> !</span>
                    )}
                  </span>
                </td>
                <td>
                  <span className={`badge ${l.access_granted ? 'badge-success' : 'badge-danger'}`}>
                    {l.access_granted ? 'GECIS VERILDI' : 'REDDEDILDI'}
                  </span>
                </td>
                <td>
                  {l.image_path
                    ? <a
                        className="img-link"
                        href={imageUrl(l.image_path)}
                        target="_blank"
                        rel="noreferrer"
                      >
                        Goruntule
                      </a>
                    : '—'
                  }
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      <p className="record-count">
        {loading ? 'Yukleniyor...' : `${logs.length} kayit`}
      </p>
    </div>
  )
}

export default LogsPage
