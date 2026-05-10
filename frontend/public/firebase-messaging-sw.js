/* global importScripts, firebase */
importScripts('https://www.gstatic.com/firebasejs/10.13.2/firebase-app-compat.js')
importScripts('https://www.gstatic.com/firebasejs/10.13.2/firebase-messaging-compat.js')

const swUrl = new URL(self.location.href)
const firebaseConfig = {
  apiKey: swUrl.searchParams.get('apiKey') || '',
  authDomain: swUrl.searchParams.get('authDomain') || '',
  projectId: swUrl.searchParams.get('projectId') || '',
  messagingSenderId: swUrl.searchParams.get('messagingSenderId') || '',
  appId: swUrl.searchParams.get('appId') || '',
}

if (firebaseConfig.apiKey && firebaseConfig.projectId && firebaseConfig.messagingSenderId && firebaseConfig.appId) {
  firebase.initializeApp(firebaseConfig)
  const messaging = firebase.messaging()

  messaging.onBackgroundMessage((payload) => {
    const title = payload?.notification?.title || 'ANPR Bildirimi'
    const body = payload?.notification?.body || 'Yeni gecis olayi'
    self.registration.showNotification(title, {
      body,
      data: payload?.data || {},
    })
  })
}
