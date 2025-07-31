// Add connection state management
enum class ConnectionState { Disconnected, Connecting, Connected, Failed, Reconnecting };

struct Remote {
    sockaddr_in addr{};
    Protocol proto = Protocol::UDP;
    int tcp_fd = -1;
    ConnectionState state = ConnectionState::Disconnected;
    std::chrono::steady_clock::time_point last_attempt{};
    int retry_count = 0;
};
