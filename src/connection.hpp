#pragma once
namespace sk150 {
enum class ConnectionState { Disconnected, Connecting, Connected, Error, Stopping };
inline ConnectionState connectionState(bool hasSession, bool stopped, bool done,
                                       bool attempted, bool fresh, bool communicationsOk) {
    if(!hasSession || (stopped&&done)) return ConnectionState::Disconnected;
    if(stopped) return ConnectionState::Stopping;
    if(done) return ConnectionState::Error;
    if(!attempted) return ConnectionState::Connecting;
    return fresh&&communicationsOk ? ConnectionState::Connected : ConnectionState::Error;
}
}
