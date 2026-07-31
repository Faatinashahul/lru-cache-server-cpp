#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <string>
#include <sstream>
#include <vector>
#include "thread_safe_lru_cache.h"

// Text protocol, loosely inspired by Redis's simplicity (not RESP itself,
// a plain whitespace-delimited protocol so it's easy to test with `nc`
// or `telnet` without a special client):
//
//   SET <key> <value>   -> "OK"
//   GET <key>           -> "<value>" or "(nil)"
//   DELETE <key>        -> "OK" or "(nil)"
//   PING                -> "PONG"
//
// Each command arrives as one line (terminated by \n) and one response
// line is returned.
class Protocol {
public:
    explicit Protocol(ThreadSafeLRUCache& cache) : cache_(cache) {}

    std::string handleCommand(const std::string& line) {
        std::istringstream iss(line);
        std::vector<std::string> tokens;
        std::string tok;
        while (iss >> tok) tokens.push_back(tok);

        if (tokens.empty()) {
            return "ERR empty command";
        }

        std::string cmd = tokens[0];
        for (auto& c : cmd) c = toupper(static_cast<unsigned char>(c));

        if (cmd == "PING") {
            return "PONG";
        } else if (cmd == "SET") {
            if (tokens.size() < 3) return "ERR usage: SET <key> <value>";
            // Allow values containing spaces: join everything after the key.
            std::string value = line.substr(line.find(tokens[2]));
            cache_.put(tokens[1], value);
            return "OK";
        } else if (cmd == "GET") {
            if (tokens.size() < 2) return "ERR usage: GET <key>";
            std::string value;
            if (cache_.get(tokens[1], value)) return value;
            return "(nil)";
        } else if (cmd == "DELETE") {
            if (tokens.size() < 2) return "ERR usage: DELETE <key>";
            return cache_.remove(tokens[1]) ? "OK" : "(nil)";
        }

        return "ERR unknown command '" + cmd + "'";
    }

private:
    ThreadSafeLRUCache& cache_;
};

#endif // PROTOCOL_H
