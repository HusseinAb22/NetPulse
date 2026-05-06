#ifndef NETPULSE_LINE_FRAMER_H
#define NETPULSE_LINE_FRAMER_H
#include <string>
#include <string_view>

class LineFramer {
public:
    template <typename Fn>
    void feed(std::string_view chunk, Fn &&on_line) {
        // 1. Append the new incoming bytes to our internal buffer
        buffer_.append(chunk);

        size_t pos;
        // 2. Keep looking for '\n' as long as one exists in the buffer
        while ((pos = buffer_.find('\n')) != std::string::npos) {
            // Extract the line (excluding the \n itself)
            std::string_view line = std::string_view(buffer_).substr(0, pos);

            // Leniency: If the client sent \r\n (Windows style), strip the \r
            // too
            if (!line.empty() && line.back() == '\r') {
                line.remove_suffix(1);
            }

            // Fire the callback with the clean line
            on_line(line);

            // Erase the emitted line and the '\n' from the buffer
            buffer_.erase(0, pos + 1);
        }
    }

    [[nodiscard]] std::size_t size() const { return buffer_.size(); }

private:
    std::string buffer_;
};

#endif  // NETPULSE_LINE_FRAMER_H
