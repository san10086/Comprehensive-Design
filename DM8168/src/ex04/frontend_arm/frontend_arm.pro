TEMPLATE = subdirs
SUBDIRS = ipc_peer app
ipc_peer.file = ipc_peer/ipc_peer.pro
app.file = app/app.pro
app.depends = ipc_peer
