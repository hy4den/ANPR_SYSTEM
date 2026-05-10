import { initializeApp } from 'firebase/app'
import { getMessaging, getToken, isSupported, onMessage } from 'firebase/messaging'
import { registerPushToken } from './api'

const firebaseConfig = {
  apiKey: import.meta.env.VITE_FIREBASE_API_KEY,
  authDomain: import.meta.env.VITE_FIREBASE_AUTH_DOMAIN,
  projectId: import.meta.env.VITE_FIREBASE_PROJECT_ID,
  messagingSenderId: import.meta.env.VITE_FIREBASE_MESSAGING_SENDER_ID,
  appId: import.meta.env.VITE_FIREBASE_APP_ID,
}

const vapidKey = import.meta.env.VITE_FIREBASE_VAPID_KEY

let firebaseApp = null
let messaging = null

function missingFirebaseConfig() {
  return !firebaseConfig.apiKey ||
    !firebaseConfig.projectId ||
    !firebaseConfig.messagingSenderId ||
    !firebaseConfig.appId ||
    !vapidKey
}

function detectPlatform() {
  const ua = navigator.userAgent.toLowerCase()
  if (ua.includes('iphone') || ua.includes('ipad') || ua.includes('ipod')) return 'ios-pwa'
  if (ua.includes('android')) return 'android-web'
  return 'web'
}

function serviceWorkerUrl() {
  const params = new URLSearchParams({
    apiKey: firebaseConfig.apiKey || '',
    authDomain: firebaseConfig.authDomain || '',
    projectId: firebaseConfig.projectId || '',
    messagingSenderId: firebaseConfig.messagingSenderId || '',
    appId: firebaseConfig.appId || '',
  })
  return `/firebase-messaging-sw.js?${params.toString()}`
}

async function ensureMessaging() {
  if (!firebaseApp) firebaseApp = initializeApp(firebaseConfig)
  if (!messaging) messaging = getMessaging(firebaseApp)
  return messaging
}

export async function enableMobileNotifications() {
  if (missingFirebaseConfig()) {
    throw new Error('Firebase env ayarlari eksik')
  }

  if (!('serviceWorker' in navigator) || !('Notification' in window)) {
    throw new Error('Bu tarayici push bildirimlerini desteklemiyor')
  }

  if (!(await isSupported())) {
    throw new Error('Bu tarayici/Firebase kombinasyonu desteklenmiyor')
  }

  const permission = await Notification.requestPermission()
  if (permission !== 'granted') {
    throw new Error('Bildirim izni verilmedi')
  }

  const registration = await navigator.serviceWorker.register(serviceWorkerUrl())
  const m = await ensureMessaging()
  const token = await getToken(m, {
    vapidKey,
    serviceWorkerRegistration: registration,
  })

  if (!token) throw new Error('FCM token alinamadi')

  await registerPushToken(token, detectPlatform())
  return token
}

export async function setupForegroundNotificationHandler(onPayload) {
  if (missingFirebaseConfig()) return
  if (!(await isSupported())) return
  const m = await ensureMessaging()
  onMessage(m, (payload) => onPayload(payload))
}
