const BASE = '/api'
const ADMIN_TOKEN = import.meta.env.VITE_ADMIN_API_TOKEN || 'anpr-dev-admin-token'

function withAdminAuth(headers = {}) {
  return { ...headers, 'X-Admin-Token': ADMIN_TOKEN }
}

async function json(response) {
  const r = await response
  if (!r.ok) {
    let msg = `HTTP ${r.status}`
    try { msg = (await r.json()).error || msg } catch {}
    throw new Error(msg)
  }
  return r.json()
}

export const getLogs = (params = {}) => {
  const clean = Object.fromEntries(
    Object.entries(params).filter(([, v]) => v !== '' && v !== undefined)
  )
  const qs = new URLSearchParams(clean).toString()
  return json(fetch(`${BASE}/logs${qs ? '?' + qs : ''}`, {
    headers: withAdminAuth(),
  }))
}

export const getWatchlist = () =>
  json(fetch(`${BASE}/watchlist`, {
    headers: withAdminAuth(),
  }))

export const addPlate = (data) =>
  json(fetch(`${BASE}/watchlist`, {
    method: 'POST',
    headers: withAdminAuth({ 'Content-Type': 'application/json' }),
    body: JSON.stringify(data),
  }))

export const removePlate = (plate) =>
  json(fetch(`${BASE}/watchlist/${encodeURIComponent(plate)}`, {
    method: 'DELETE',
    headers: withAdminAuth(),
  }))

export const imageUrl = (imagePath) => {
  const filename = imagePath?.split('/').pop()
  if (!filename) return ''
  return `${BASE}/images/${encodeURIComponent(filename)}?admin_token=${encodeURIComponent(ADMIN_TOKEN)}`
}

export const registerPushToken = (token, platform = 'web') =>
  json(fetch(`${BASE}/notifications/register`, {
    method: 'POST',
    headers: withAdminAuth({ 'Content-Type': 'application/json' }),
    body: JSON.stringify({ token, platform }),
  }))

export const unregisterPushToken = (token) =>
  json(fetch(`${BASE}/notifications/unregister`, {
    method: 'POST',
    headers: withAdminAuth({ 'Content-Type': 'application/json' }),
    body: JSON.stringify({ token }),
  }))
