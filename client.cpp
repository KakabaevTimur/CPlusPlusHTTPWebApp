#include <iostream>

#include "src/client.hpp"

using enum HTTPMethod;

Response res{};

void registerUser(Client &c, std::string login, std::string password, std::string role);

void loginUser(Client &c, std::string login, std::string password);

std::string getData(Client &c);

int main()
{
    using namespace std;
    Client client{};

    cout << "Do you want to register or login?" << endl;
    cout << "Type 1 if you want to register or 2 if you want to login" << endl;
    int userChoice;
    string login, password, role;
    cin >> userChoice;
    cout << "Enter your login" << endl;
    cin >> login;
    cout << "Enter your password" << endl;
    cin >> password;
    if (userChoice == 1)
    {
        cout << "Enter your role" << endl;
        cin >> role;
        registerUser(client, login, password, role);        
    }
    else
    {
        loginUser(client, login, password);
    }

    cout << "You accessed the data: " << getData(client) << endl;
}

void registerUser(Client &c, std::string login, std::string password, std::string role)
{
    std::string params = "login=" + login + "&pass=" + password + "&role=" + role;
    Request req{GET, "/register", params.c_str()};
    c.request(req);
    res = c.getResponce();
    if (res.status != 200)
    {
        exit(1);
    }
};

void loginUser(Client &c, std::string login, std::string password)
{
    std::string params = "login=" + login + "&pass=" + password;
    Request req{GET, "/login", params.c_str()};
    c.request(req);
    res = c.getResponce();
    if (res.status != 200)
    {
        exit(1);
    }
}

std::string getData(Client &c)
{
    Request req2{GET, "/data"};
    c.request(req2);

    res = c.getResponce();
    return res.content;
}
