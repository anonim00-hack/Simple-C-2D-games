#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

class ChatServer
{
  private:
    const std::string ChatFile = "chat_hystory.txt";

    void writeToFile(const std::string &ip, const std::string &msg)
    {
        std::ofstream outfile(ChatFile, std::ios_base::app);
        if (outfile.is_open())
        {
            outfile << ip << ":" << msg << std::endl;
            outfile.close();
        }
    }

    void readFile(std::vector<std::string> &hystory)
    {
        hystory.clear();
        std::ifstream infile(ChatFile);
        std::string line;
        while (std::getline(infile, line))
        {
            if (!line.empty())
                hystory.push_back(line);
        }
    }
    std::string decodeUTF8(std::string str)
    {
        std::string res;
        for (size_t i = 0; i < str.length(); ++i)
        {
            if (str[i] == '+')
                res += ' ';
            else if (str[i] == '%' && i + 2 < str.length())
            {
                int value;
                sscanf(str.substr(i + 1, 2).c_str(), "%x", &value);
                res += static_cast<char>(value);
                i += 2;
            } else
                res += str[i];
        }
        return res;
    }
    std::string formatMessage(
        const std::string &line, const std::string &viewer_ip)
    {
        size_t delimiter = line.find(":");
        if (delimiter == std::string::npos)
            return "";

        std::string sender_ip = line.substr(0, delimiter);
        std::string message = line.substr(delimiter + 1);

        bool is_my = (sender_ip == viewer_ip);

        std::string align = is_my ? "align-self: flex-end; background:#dcf8c6;"
                                  : "align-self: flex-start; background:white;";

        return "<div style='" + align +
               " padding:8px 12px; margin-bottom:5px; border-radius:15px; "
               "box-shadow: 0 1px 2px rgba(0,0,0,0.1); width:fit-content; "
               "max-width:80%; word-wrap: break-word;'>"+ message + "</div>";
    }
    sockaddr_in address;
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

  public:
    int opt = 1;
    ChatServer(int PORT)
    {
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(PORT);

        if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
        {
            std::cerr << "Ошибка bind: порт занят!" << std::endl;
            exit(1);
        }
        listen(server_fd, 5);
        std::cout << "Сервер готов на порту 8080" << std::endl;
    }

    void start()
    {
        while (true)
        {
            sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);


            int client_socket =
                accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
            if (client_socket < 0)
                continue;

            std::string client_ip = inet_ntoa(client_addr.sin_addr);
            char buffer[4096] = {0};
            read(client_socket, buffer, 4096);
            std::string request(buffer);

            // --- ПАРСИМ ПУТЬ ---
            size_t first_space = request.find(" ");
            size_t second_space = request.find(" ", first_space + 1);
            if (first_space == std::string::npos ||
                second_space == std::string::npos)
            {
                close(client_socket);
                continue;
            }
            std::string path =
                request.substr(first_space + 1, second_space - first_space - 1);

            // Объявляем вектор истории один раз для всего цикла
            std::vector<std::string> hystory;

            // --- ЛОГИКА API (Только сообщения) ---
            if (path == "/api/messages")
            {
                readFile(hystory);
                std::string only_logs = "";
                for (const auto &line : hystory)
                {
                    only_logs += formatMessage(line, client_ip);
                }

                std::string res = "HTTP/1.1 200 OK\r\nContent-Type: text/html; "
                                  "charset=UTF-8\r\n\r\n" +
                                  only_logs;
                write(client_socket, res.c_str(), res.length());
                close(client_socket);
                continue;
            }

            // --- ЛОГИКА POST (Прием сообщения) ---
            size_t header_end = request.find("\r\n\r\n");
            if (request.substr(0, 4) == "POST" &&
                header_end != std::string::npos)
            {
                std::string body = request.substr(header_end + 4);
                if (body.find("text=") == 0)
                {
                    std::string msg = decodeUTF8(body.substr(5));
                    if (!msg.empty())
                        writeToFile(client_ip, msg);

                    std::string response =
                        "HTTP/1.1 303 See Other\r\nLocation: /\r\n\r\n";
                    write(client_socket, response.c_str(), response.length());
                    close(client_socket);
                    continue;
                }
            }

            // --- ЛОГИКА ГЛАВНОЙ СТРАНИЦЫ (GET /) ---
            readFile(hystory);
            std::string chat_logs =
                "<div id='chat' style='display:flex; flex-direction:column; "
                "gap:8px; border:1px solid #ddd; padding:15px; "
                "border-radius:10px; "
                "height:400px; overflow-y:auto; background:#f4f7f6;'>";
            for (const auto &line : hystory)
            {
                chat_logs += formatMessage(line, client_ip);
            }
            chat_logs += "</div>";

            std::string html =
                "<html><head><meta charset='UTF-8'><title>C++ Chat</title>"
                "<style>body{font-family:sans-serif; display:flex; "
                "justify-content:center; background:#e5ddd5; padding:20px;}"
                ".container{width:100%; max-width:500px; background:white; "
                "padding:20px; border-radius:10px; box-shadow:0 4px 10px "
                "rgba(0,0,0,0.1);}"
                "input{flex:1; padding:12px; border:1px solid #ddd; "
                "border-radius:20px; outline:none;}"
                "button{padding:10px 20px; background:#075e54; "
                "color:white; border:none; border-radius:20px; "
                "cursor:pointer; font-weight:bold;}</style>"
                "</head><body><div class='container'><h2>💬 C++ "
                "Messenger</h2>" +
                chat_logs +
                "<form method='POST' style='margin-top:15px; display:flex; "
                "gap:10px;'>"
                "<input type='text' name='text' placeholder='Напишите...' "
                "autocomplete='off' required>"
                "<button type='submit'>></button></form></div>"
                "<script>"
                "var c = document.getElementById('chat'); c.scrollTop = "
                "c.scrollHeight;"
                "setInterval(function() {"
                "  fetch('/api/messages').then(r => r.text()).then(data => "
                "{" // ТУТ ИЗМЕНИЛ НА /api/messages
                "    var oldChat = document.getElementById('chat');"
                "    if (oldChat.innerHTML !== data) {" // Упростил
                                                        // сравнение
                "      oldChat.innerHTML = data; oldChat.scrollTop = "
                "oldChat.scrollHeight;"
                "    }"
                "  });"
                "}, 1000);" // Раз в секунду — теперь это не грузит сервер
                "</script></body></html>";

            std::string response =
                "HTTP/1.1 200 OK\r\nContent-Type: text/html; "
                "charset=UTF-8\r\nContent-Length: " +
                std::to_string(html.length()) + "\r\n\r\n" + html;
            write(client_socket, response.c_str(), response.length());
            close(client_socket);
        }
    }
};

int main()
{
    ChatServer chat(8080);
    chat.start();
    return 0;
}