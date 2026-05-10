import { useState, useEffect } from 'react'
import { getWatchlist, addPlate, removePlate } from '../api'

const CATEGORIES = ['VIP', 'Staff', 'Blacklisted', 'Resident']
const CAT_CLASS   = {
  VIP:         'cat-vip',
  Staff:       'cat-staff',
  Blacklisted: 'cat-blacklisted',
  Resident:    'cat-resident',
}

const EMPTY_FORM = {
  license_plate: '',
  owner_name: '',
  owner_email: '',
  category: 'Resident',
  auto_grant: false,
}

function WatchlistPage() {
  const [list, setList]           = useState([])
  const [loading, setLoading]     = useState(false)
  const [form, setForm]           = useState(EMPTY_FORM)
  const [submitting, setSubmitting] = useState(false)
  const [error, setError]         = useState(null)

  const fetchList = async () => {
    setLoading(true)
    setError(null)
    try { setList(await getWatchlist()) }
    catch (e) { setError(e.message) }
    finally { setLoading(false) }
  }

  useEffect(() => { fetchList() }, [])

  const handleChange = e => {
    const { name, value, type, checked } = e.target
    setForm(f => ({ ...f, [name]: type === 'checkbox' ? checked : value }))
  }

  const handleAdd = async e => {
    e.preventDefault()
    const plate = form.license_plate.trim().toUpperCase()
    if (!plate) return
    setSubmitting(true)
    setError(null)
    try {
      await addPlate({ ...form, license_plate: plate })
      setForm(EMPTY_FORM)
      await fetchList()
    } catch (e) {
      setError(e.message)
    } finally {
      setSubmitting(false)
    }
  }

  const handleRemove = async (plate) => {
    // eslint-disable-next-line no-restricted-globals
    if (!confirm(`"${plate}" plakalı araci listeden sil?`)) return
    setError(null)
    try {
      await removePlate(plate)
      setList(l => l.filter(e => e.license_plate !== plate))
    } catch (e) {
      setError(e.message)
    }
  }

  return (
    <div>
      <div className="page-header">
        <h2>Izin Listesi (Watchlist)</h2>
        <span className="record-count">{list.length} kayitli arac</span>
      </div>

      {error && <div className="alert alert-error">{error}</div>}

      {/* Add plate form */}
      <form className="add-form" onSubmit={handleAdd}>
        <h3>Yeni Arac Ekle</h3>
        <div className="form-row">
          <input
            className="input"
            style={{ minWidth: 160 }}
            placeholder="Plaka (34ABC123)"
            name="license_plate"
            value={form.license_plate}
            onChange={handleChange}
            required
            maxLength={15}
          />
          <input
            className="input"
            style={{ minWidth: 160 }}
            placeholder="Arac Sahibi"
            name="owner_name"
            value={form.owner_name}
            onChange={handleChange}
          />
          <input
            className="input"
            style={{ minWidth: 200 }}
            placeholder="Sahip E-posta (bildirim icin)"
            type="email"
            name="owner_email"
            value={form.owner_email}
            onChange={handleChange}
          />
          <select className="input" name="category" value={form.category} onChange={handleChange}>
            {CATEGORIES.map(c => <option key={c} value={c}>{c}</option>)}
          </select>
          <label className="checkbox-label">
            <input
              type="checkbox"
              name="auto_grant"
              checked={form.auto_grant}
              onChange={handleChange}
            />
            <span>Otomatik Ac</span>
          </label>
          <button className="btn btn-primary" type="submit" disabled={submitting}>
            {submitting ? 'Ekleniyor...' : 'Ekle'}
          </button>
        </div>
      </form>

      {/* Watchlist table */}
      <div className="table-wrap">
        <table className="table">
          <thead>
            <tr>
              <th>Plaka</th>
              <th>Arac Sahibi</th>
              <th>Sahip E-posta</th>
              <th>Kategori</th>
              <th>Otomatik Ac</th>
              <th></th>
            </tr>
          </thead>
          <tbody>
            {loading ? (
              <tr><td colSpan={6} className="td-center">Yukleniyor...</td></tr>
            ) : list.length === 0 ? (
              <tr><td colSpan={6} className="td-center">Liste bos — yukardaki formdan arac ekleyin</td></tr>
            ) : list.map(e => (
              <tr key={e.plate_id}>
                <td><span className="plate-chip">{e.license_plate}</span></td>
                <td>{e.owner_name || '—'}</td>
                <td>{e.owner_email || '—'}</td>
                <td>
                  <span className={`cat-badge ${CAT_CLASS[e.category] || ''}`}>
                    {e.category}
                  </span>
                </td>
                <td>
                  <span className={`badge ${e.auto_grant ? 'badge-success' : 'badge-neutral'}`}>
                    {e.auto_grant ? 'EVET' : 'HAYIR'}
                  </span>
                </td>
                <td>
                  <button
                    className="btn btn-danger btn-sm"
                    onClick={() => handleRemove(e.license_plate)}
                  >
                    Sil
                  </button>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      {/* Category legend */}
      <div style={{ marginTop: 16, display: 'flex', gap: 12, flexWrap: 'wrap' }}>
        {CATEGORIES.map(c => (
          <span key={c} className={`cat-badge ${CAT_CLASS[c]}`}>{c}</span>
        ))}
        <span style={{ fontSize: 12, color: 'var(--muted)', alignSelf: 'center' }}>
          — Blacklisted kategorisi her zaman reddedilir
        </span>
      </div>
    </div>
  )
}

export default WatchlistPage
