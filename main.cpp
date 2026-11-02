
#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <iomanip>
#include <cmath>
#include <cstdlib>
#include <cctype>

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

// ==========================================
// SMART CITY EMERGENCY COMMAND CENTER
// ==========================================

const int N = 7;
const int PORT = 8080;

const string DATA_FILE = "smartcity_data.txt";

// ==========================================
// CITY LOCATIONS
// ==========================================

string city[N] = {
    "Central",
    "City Hospital",
    "Police Station",
    "Fire Station",
    "Airport",
    "Market",
    "North Junction"
};

double cityLat[N] = {
    19.9975,
    20.0066,
    20.0057,
    20.0035,
    20.1190,
    20.0005,
    20.0120
};

double cityLon[N] = {
    73.7898,
    73.7824,
    73.7904,
    73.7890,
    73.9129,
    73.7895,
    73.7900
};

// ==========================================
// ROAD STRUCTURE
// ==========================================

struct Road {
    int a;
    int b;
    int distance;
    int traffic;
};

vector<Road> roads = {
    {0,1,4,0},
    {0,2,3,0},
    {1,3,2,0},
    {2,3,5,0},
    {3,4,4,0},
    {2,5,3,0},
    {5,3,2,0},
    {0,6,3,0},
    {6,1,4,0},
    {5,4,5,0}
};

// Traffic:
// 0 = Low
// 1 = Moderate
// 2 = Heavy
// 3 = Closed

// ==========================================
// EMERGENCY STRUCTURE
// ==========================================

struct Emergency {
    int id;
    int source;
    int destination;
    int severity;

    string type;
    string status;
};

vector<Emergency> incidents;

// ==========================================
// GLOBAL VARIABLES
// ==========================================

int source = 0;
int destination = 4;

int selectedRoad = 0;
int nextID = 1001;

int emergencyCounter = 0;
int trafficStep = 0;

bool fastest = false;
bool trafficSimulation = false;
bool vehicleMoving = false;

DWORD lastTrafficTick = 0;

vector<int> route;

string resultText = "Smart City Backend Ready.";

// ==========================================
// JSON HELPERS
// ==========================================

string jsonEscape(const string& s) {

    string out;

    for (char c : s) {

        if (c == '\\' || c == '"') {
            out += '\\';
            out += c;
        }
        else if (c == '\n') {
            out += "\\n";
        }
        else if (c == '\r') {
            out += "\\r";
        }
        else if ((unsigned char)c >= 32) {
            out += c;
        }
    }

    return out;
}

string jsonString(
    const string& body,
    const string& key,
    const string& fallback = ""
) {

    string needle = "\"" + key + "\"";

    size_t p = body.find(needle);

    if (p == string::npos)
        return fallback;

    p = body.find(':', p);

    if (p == string::npos)
        return fallback;

    p++;

    while (p < body.size() &&
           isspace((unsigned char)body[p]))
        p++;

    if (p >= body.size() || body[p] != '"')
        return fallback;

    p++;

    string out;
    bool escape = false;

    for (; p < body.size(); p++) {

        char c = body[p];

        if (escape) {

            if (c == 'n')
                out += '\n';

            else if (c == 'r')
                out += '\r';

            else
                out += c;

            escape = false;
        }

        else if (c == '\\') {
            escape = true;
        }

        else if (c == '"') {
            break;
        }

        else {
            out += c;
        }
    }

    return out;
}

double jsonDouble(
    const string& body,
    const string& key,
    double fallback = 0
) {

    string needle = "\"" + key + "\"";

    size_t p = body.find(needle);

    if (p == string::npos)
        return fallback;

    p = body.find(':', p);

    if (p == string::npos)
        return fallback;

    p++;

    while (p < body.size() &&
           isspace((unsigned char)body[p]))
        p++;

    try {
        return stod(body.substr(p));
    }
    catch (...) {
        return fallback;
    }
}

int jsonInt(
    const string& body,
    const string& key,
    int fallback = 0
) {

    string needle = "\"" + key + "\"";

    size_t p = body.find(needle);

    if (p == string::npos)
        return fallback;

    p = body.find(':', p);

    if (p == string::npos)
        return fallback;

    p++;

    while (p < body.size() &&
           isspace((unsigned char)body[p]))
        p++;

    try {
        return stoi(body.substr(p));
    }
    catch (...) {
        return fallback;
    }
}

bool jsonBool(
    const string& body,
    const string& key,
    bool fallback = false
) {

    string needle = "\"" + key + "\"";

    size_t p = body.find(needle);

    if (p == string::npos)
        return fallback;

    p = body.find(':', p);

    if (p == string::npos)
        return fallback;

    p++;

    while (p < body.size() &&
           isspace((unsigned char)body[p]))
        p++;

    return body.compare(p, 4, "true") == 0;
}

// ==========================================
// FIND CITY INDEX
// ==========================================

int cityIndex(const string& name) {

    for (int i = 0; i < N; i++) {

        if (city[i] == name)
            return i;
    }

    return -1;
}

// ==========================================
// FIND NEAREST NASHIK GRAPH LOCATION
// ==========================================

int nearestCity(double lat, double lon) {

    int best = 0;

    double minimum = 1e100;

    const double PI = 3.141592653589793;

    for (int i = 0; i < N; i++) {

        double dy = (lat - cityLat[i]) * 111.0;

        double dx =
            (lon - cityLon[i]) *
            111.0 *
            cos(lat * PI / 180.0);

        double distance = dx * dx + dy * dy;

        if (distance < minimum) {

            minimum = distance;
            best = i;
        }
    }

    return best;
}

// ==========================================
// SAVE DATA
// ==========================================

bool saveData(bool showMessage = true) {

    ofstream file(DATA_FILE);

    if (!file.is_open()) {

        if (showMessage)
            resultText = "Unable to save data.";

        return false;
    }

    file << "SMARTCITY1\n";

    file << nextID << " "
         << emergencyCounter << " "
         << source << " "
         << destination << " "
         << selectedRoad << "\n";

    file << roads.size() << "\n";

    for (const auto& r : roads) {

        file << r.a << " "
             << r.b << " "
             << r.distance << " "
             << r.traffic << "\n";
    }

    file << incidents.size() << "\n";

    for (const auto& e : incidents) {

        int type = 0;

        if (e.type == "Fire Brigade")
            type = 1;

        else if (e.type == "Police")
            type = 2;

        int status = 0;

        if (e.status == "Assigned")
            status = 1;

        else if (e.status == "Completed")
            status = 2;

        file << e.id << " "
             << e.source << " "
             << e.destination << " "
             << e.severity << " "
             << type << " "
             << status << "\n";
    }

    if (showMessage)
        resultText = "Data saved successfully.";

    return true;
}

// ==========================================
// LOAD DATA
// ==========================================

bool loadData(bool showMessage = true) {

    ifstream file(DATA_FILE);

    if (!file.is_open()) {

        if (showMessage)
            resultText = "No saved data found.";

        return false;
    }

    string version;

    getline(file, version);

    if (version != "SMARTCITY1") {

        if (showMessage)
            resultText = "Invalid data file.";

        return false;
    }

    int ni, ec, src, dst, sr;

    if (!(file >> ni >> ec >> src >> dst >> sr) ||
        ni < 1 ||
        ec < 0 ||
        src < 0 || src >= N ||
        dst < 0 || dst >= N ||
        sr < 0 || sr >= (int)roads.size()) {

        if (showMessage)
            resultText = "Invalid saved settings.";

        return false;
    }

    int roadCount;

    if (!(file >> roadCount) ||
        roadCount != (int)roads.size()) {

        if (showMessage)
            resultText = "Invalid road data.";

        return false;
    }

    vector<Road> tempRoads = roads;

    for (int i = 0; i < roadCount; i++) {

        int a, b, d, t;

        if (!(file >> a >> b >> d >> t) ||
            a != roads[i].a ||
            b != roads[i].b ||
            d != roads[i].distance ||
            t < 0 || t > 3) {

            if (showMessage)
                resultText = "Invalid road record.";

            return false;
        }

        tempRoads[i].traffic = t;
    }

    int count;

    if (!(file >> count) || count < 0 || count > 10000) {

        if (showMessage)
            resultText = "Invalid emergency count.";

        return false;
    }

    vector<Emergency> tempIncidents;

    int maximumID = 1000;

    for (int i = 0; i < count; i++) {

        Emergency e;

        int type, status;

        if (!(file >> e.id
              >> e.source
              >> e.destination
              >> e.severity
              >> type
              >> status) ||

            e.id < 1 ||
            e.source < 0 || e.source >= N ||
            e.destination < 0 || e.destination >= N ||
            e.severity < 1 || e.severity > 3 ||
            type < 0 || type > 2 ||
            status < 0 || status > 2) {

            if (showMessage)
                resultText = "Invalid emergency record.";

            return false;
        }

        e.type =
            type == 1 ? "Fire Brigade" :
            type == 2 ? "Police" :
            "Ambulance";

        e.status =
            status == 1 ? "Assigned" :
            status == 2 ? "Completed" :
            "Pending";

        maximumID = max(maximumID, e.id);

        tempIncidents.push_back(e);
    }

    roads = tempRoads;
    incidents = tempIncidents;

    nextID = max(ni, maximumID + 1);

    emergencyCounter = ec;

    source = src;
    destination = dst;
    selectedRoad = sr;

    route.clear();

    vehicleMoving = false;

    if (showMessage)
        resultText = "Data loaded successfully.";

    return true;
}

// ==========================================
// TRAFFIC COST
// ==========================================

double roadCost(const Road& r) {

    if (r.traffic == 3)
        return 1e9;

    if (!fastest)
        return r.distance;

    double speed;

    if (r.traffic == 0)
        speed = 40;

    else if (r.traffic == 1)
        speed = 25;

    else
        speed = 12;

    return (r.distance / speed) * 60.0;
}

// ==========================================
// DIJKSTRA ALGORITHM
// ==========================================

bool findRoute(int s, int d) {

    double distance[N];

    int parent[N];

    bool visited[N] = {};

    for (int i = 0; i < N; i++) {

        distance[i] = 1e9;

        parent[i] = -1;
    }

    distance[s] = 0;

    for (int k = 0; k < N; k++) {

        int u = -1;

        for (int i = 0; i < N; i++) {

            if (!visited[i] &&
                (u == -1 || distance[i] < distance[u])) {

                u = i;
            }
        }

        if (u == -1 || distance[u] >= 1e9)
            break;

        visited[u] = true;

        for (const auto& r : roads) {

            int v = -1;

            if (r.a == u)
                v = r.b;

            else if (r.b == u)
                v = r.a;

            if (v != -1 && !visited[v]) {

                double newDistance =
                    distance[u] + roadCost(r);

                if (newDistance < distance[v]) {

                    distance[v] = newDistance;

                    parent[v] = u;
                }
            }
        }
    }

    route.clear();

    if (distance[d] >= 1e9) {

        resultText =
            "No route available. Check closed roads.";

        vehicleMoving = false;

        return false;
    }

    for (int v = d; v != -1; v = parent[v])
        route.push_back(v);

    reverse(route.begin(), route.end());

    double km = 0;

    for (size_t i = 0; i + 1 < route.size(); i++) {

        for (const auto& r : roads) {

            if ((r.a == route[i] &&
                 r.b == route[i + 1]) ||

                (r.b == route[i] &&
                 r.a == route[i + 1])) {

                km += r.distance;

                break;
            }
        }
    }

    stringstream ss;

    ss << (fastest ?
        "FASTEST GRAPH ROUTE\n" :
        "SHORTEST GRAPH ROUTE\n");

    for (size_t i = 0; i < route.size(); i++) {

        if (i > 0)
            ss << " -> ";

        ss << city[route[i]];
    }

    ss << fixed << setprecision(2);

    ss << "\nDistance: " << km << " km";

    if (fastest)
        ss << "\nEstimated time: "
           << distance[d] << " minutes";

    resultText = ss.str();

    return true;
}

// ==========================================
// INCIDENT COUNTS
// ==========================================

int activeCount() {

    int count = 0;

    for (const auto& e : incidents) {

        if (e.status != "Completed")
            count++;
    }

    return count;
}

int criticalCount() {

    int count = 0;

    for (const auto& e : incidents) {

        if (e.status != "Completed" &&
            e.severity >= 3) {

            count++;
        }
    }

    return count;
}

// ==========================================
// ADD EMERGENCY
// ==========================================

void addIncident() {

    if (source == destination) {

        resultText =
            "Source and destination cannot be same.";

        return;
    }

    Emergency e;

    e.id = nextID++;

    e.source = source;

    e.destination = destination;

    emergencyCounter++;

    if (emergencyCounter % 3 == 1) {

        e.type = "Ambulance";

        e.severity = 3;
    }

    else if (emergencyCounter % 3 == 2) {

        e.type = "Fire Brigade";

        e.severity = 2;
    }

    else {

        e.type = "Police";

        e.severity = 1;
    }

    e.status = "Pending";

    incidents.push_back(e);

    resultText =
        "Emergency ER-" + to_string(e.id) +
        " added successfully.";

    saveData(false);
}

// ==========================================
// MAX HEAP PRIORITY
// ==========================================

bool higherPriority(int a, int b) {

    if (incidents[a].severity !=
        incidents[b].severity) {

        return incidents[a].severity >
               incidents[b].severity;
    }

    return incidents[a].id <
           incidents[b].id;
}

void heapifyDown(vector<int>& heap, int i) {

    int largest = i;

    int left = 2 * i + 1;

    int right = 2 * i + 2;

    if (left < (int)heap.size() &&
        higherPriority(heap[left], heap[largest])) {

        largest = left;
    }

    if (right < (int)heap.size() &&
        higherPriority(heap[right], heap[largest])) {

        largest = right;
    }

    if (largest != i) {

        swap(heap[i], heap[largest]);

        heapifyDown(heap, largest);
    }
}

// ==========================================
// PROCESS EMERGENCY
// ==========================================

void processIncident() {

    vector<int> heap;

    for (int i = 0; i < (int)incidents.size(); i++) {

        if (incidents[i].status == "Pending")
            heap.push_back(i);
    }

    if (heap.empty()) {

        resultText =
            "No pending emergency requests.";

        return;
    }

    for (int i = (int)heap.size() / 2 - 1;
         i >= 0; i--) {

        heapifyDown(heap, i);
    }

    int best = heap[0];

    incidents[best].status = "Assigned";

    source = incidents[best].source;

    destination = incidents[best].destination;

    fastest = true;

    vehicleMoving = false;

    if (findRoute(source, destination)) {

        vehicleMoving = route.size() >= 2;

        resultText +=
            "\nMAX-HEAP selected ER-" +
            to_string(incidents[best].id);
    }

    saveData(false);
}

// ==========================================
// COMPLETE EMERGENCY
// ==========================================

void completeIncident(const string& id = "") {

    if (!id.empty()) {

        int number = atoi(
            id.substr(3).c_str()
        );

        for (auto& e : incidents) {

            if (e.id == number &&
                e.status != "Completed") {

                e.status = "Completed";

                vehicleMoving = false;

                resultText =
                    id + " marked Completed.";

                saveData(false);

                return;
            }
        }

        resultText = "Incident not found.";

        return;
    }

    for (int i = (int)incidents.size() - 1;
         i >= 0; i--) {

        if (incidents[i].status == "Assigned") {

            incidents[i].status = "Completed";

            vehicleMoving = false;

            resultText =
                "Emergency marked Completed.";

            saveData(false);

            return;
        }
    }

    resultText =
        "No assigned emergency to complete.";
}

// ==========================================
// TRAFFIC SIMULATION
// ==========================================

void updateTraffic() {

    if (roads.empty())
        return;

    trafficStep =
        (trafficStep + 1) % (int)roads.size();

    roads[trafficStep].traffic =
        (roads[trafficStep].traffic + 1) % 4;

    selectedRoad = trafficStep;

    fastest = true;

    findRoute(source, destination);

    saveData(false);
}

// ==========================================
// JSON STATE FOR FRONTEND
// ==========================================

string stateJson() {

    stringstream s;

    s << "{\"ok\":true";

    s << ",\"result\":\""
      << jsonEscape(resultText) << "\"";

    s << ",\"source\":\""
      << jsonEscape(city[source]) << "\"";

    s << ",\"destination\":\""
      << jsonEscape(city[destination]) << "\"";

    s << ",\"fastest\":"
      << (fastest ? "true" : "false");

    s << ",\"vehicleMoving\":"
      << (vehicleMoving ? "true" : "false");

    s << ",\"activeCount\":"
      << activeCount();

    s << ",\"criticalCount\":"
      << criticalCount();

    // LOCATIONS

    s << ",\"locations\":[";

    for (int i = 0; i < N; i++) {

        if (i)
            s << ",";

        s << "{\"id\":" << i
          << ",\"name\":\""
          << jsonEscape(city[i]) << "\"}";
    }

    s << "]";

    // ROADS

    s << ",\"roads\":[";

    for (size_t i = 0; i < roads.size(); i++) {

        if (i)
            s << ",";

        const auto& r = roads[i];

        s << "{\"a\":" << r.a
          << ",\"b\":" << r.b
          << ",\"distance\":" << r.distance
          << ",\"traffic\":" << r.traffic
          << "}";
    }

    s << "]";

    // ROUTE

    s << ",\"route\":[";

    for (size_t i = 0; i < route.size(); i++) {

        if (i)
            s << ",";

        s << route[i];
    }

    s << "]";

    // INCIDENTS

    s << ",\"incidents\":[";

    for (size_t i = 0; i < incidents.size(); i++) {

        if (i)
            s << ",";

        const auto& e = incidents[i];

        s << "{\"id\":\"ER-" << e.id
          << "\",\"number\":" << e.id
          << ",\"source\":\""
          << jsonEscape(city[e.source])
          << "\",\"destination\":\""
          << jsonEscape(city[e.destination])
          << "\",\"type\":\""
          << jsonEscape(e.type)
          << "\",\"priority\":" << e.severity
          << ",\"severity\":" << e.severity
          << ",\"status\":\""
          << jsonEscape(e.status)
          << "\"}";
    }

    s << "]}";

    return s.str();
}

// ==========================================
// HANDLE FRONTEND COMMANDS
// ==========================================

void handleCommand(const string& body) {

    string action = jsonString(body, "action");

    // ADD EMERGENCY

    if (action == "add") {

        string sn = jsonString(body, "source");

        string dn = jsonString(body, "destination");

        int si = cityIndex(sn);

        int di = cityIndex(dn);

        if (si < 0) {

            double lat =
                jsonDouble(body, "sourceLat", -999);

            double lon =
                jsonDouble(body, "sourceLon", -999);

            if (lat < -90 || lat > 90 ||
                lon < -180 || lon > 180) {

                resultText =
                    "Source coordinates missing. Select search suggestion.";

                return;
            }

            si = nearestCity(lat, lon);
        }

        if (di < 0) {

            double lat =
                jsonDouble(body, "destinationLat", -999);

            double lon =
                jsonDouble(body, "destinationLon", -999);

            if (lat < -90 || lat > 90 ||
                lon < -180 || lon > 180) {

                resultText =
                    "Destination coordinates missing. Select search suggestion.";

                return;
            }

            di = nearestCity(lat, lon);
        }

        source = si;

        destination = di;

        if (source == destination) {

            resultText =
                "Both locations map to same graph node. Choose locations farther apart.";

            return;
        }

        addIncident();

        resultText +=
            "\nSearched locations mapped approximately to: " +
            city[source] + " -> " + city[destination];
    }

    // PROCESS EMERGENCY

    else if (action == "process") {

        processIncident();
    }

    // COMPLETE EMERGENCY

    else if (action == "complete") {

        completeIncident(
            jsonString(body, "id")
        );
    }

    // TRAFFIC CONTROL

    else if (action == "traffic") {

        int road = jsonInt(body, "road", -1);

        int state = jsonInt(body, "state", -1);

        if (road >= 0 &&
            road < (int)roads.size() &&
            state >= 0 &&
            state <= 3) {

            roads[road].traffic = state;

            selectedRoad = road;

            fastest = true;

            findRoute(source, destination);

            resultText =
                "Traffic condition updated for Road " +
                to_string(road + 1);

            saveData(false);
        }

        else {

            resultText = "Invalid traffic selection.";
        }
    }

    // TRAFFIC SIMULATION

    else if (action == "simulation") {

        trafficSimulation =
            jsonBool(body, "enabled", false);

        lastTrafficTick = GetTickCount();

        resultText =
            trafficSimulation ?
            "Traffic simulation started." :
            "Traffic simulation stopped.";
    }

    // SAVE

    else if (action == "save") {

        saveData(true);
    }

    // LOAD

    else if (action == "load") {

        loadData(true);
    }

    // GRAPH ROUTE

    else if (action == "route") {

        string sn = jsonString(body, "source");

        string dn = jsonString(body, "destination");

        int si = cityIndex(sn);

        int di = cityIndex(dn);

        if (si >= 0)
            source = si;

        if (di >= 0)
            destination = di;

        fastest =
            jsonString(body, "mode") == "fastest";

        findRoute(source, destination);

        saveData(false);
    }
}

// ==========================================
// READ HTML FILE
// ==========================================

string readFile(const string& path) {

    ifstream file(path, ios::binary);

    if (!file)
        return "";

    ostringstream ss;

    ss << file.rdbuf();

    return ss.str();
}

// ==========================================
// HTTP RESPONSE
// ==========================================

void sendResponse(
    SOCKET client,
    int status,
    const string& contentType,
    const string& body
) {

    string statusText =
        status == 200 ? "OK" :
        status == 404 ? "Not Found" :
        "Bad Request";

    stringstream h;

    h << "HTTP/1.1 "
      << status << " "
      << statusText << "\r\n";

    h << "Content-Type: "
      << contentType << "\r\n";

    h << "Content-Length: "
      << body.size() << "\r\n";

    h << "Access-Control-Allow-Origin: *\r\n";

    h << "Access-Control-Allow-Headers: Content-Type\r\n";

    h << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";

    h << "Connection: close\r\n\r\n";

    string response = h.str() + body;

    send(
        client,
        response.c_str(),
        (int)response.size(),
        0
    );
}

// ==========================================
// HTTP CLIENT HANDLER
// ==========================================

void serveClient(SOCKET client) {

    string request;

    char buffer[8192];

    int n = recv(
        client,
        buffer,
        sizeof(buffer),
        0
    );

    if (n <= 0)
        return;

    request.assign(buffer, n);

    size_t lineEnd = request.find("\r\n");

    if (lineEnd == string::npos) {

        sendResponse(
            client,
            400,
            "text/plain",
            "Bad request"
        );

        return;
    }

    string firstLine =
        request.substr(0, lineEnd);

    istringstream fs(firstLine);

    string method, path, version;

    fs >> method >> path >> version;

    // OPTIONS REQUEST

    if (method == "OPTIONS") {

        sendResponse(
            client,
            200,
            "text/plain",
            ""
        );

        return;
    }

    // DASHBOARD

    if (path == "/" || path == "/index.html") {

        string html = readFile("index.html");

        if (html.empty()) {

            sendResponse(
                client,
                404,
                "text/plain",
                "index.html not found. Keep index.html with main.cpp."
            );
        }

        else {

            sendResponse(
                client,
                200,
                "text/html; charset=utf-8",
                html
            );
        }

        return;
    }

    // STATE API

    if (path == "/api/state") {

        sendResponse(
            client,
            200,
            "application/json; charset=utf-8",
            stateJson()
        );

        return;
    }

    // COMMAND API

    if (path == "/api/command" &&
        method == "POST") {

        size_t headerEnd =
            request.find("\r\n\r\n");

        if (headerEnd == string::npos) {

            sendResponse(
                client,
                400,
                "application/json",
                "{\"ok\":false}"
            );

            return;
        }

        string headers =
            request.substr(0, headerEnd);

        int contentLength = 0;

        size_t cl =
            headers.find("Content-Length:");

        if (cl != string::npos) {

            contentLength =
                atoi(headers.c_str() + cl + 15);
        }

        string body =
            request.substr(headerEnd + 4);

        while ((int)body.size() < contentLength) {

            int m = recv(
                client,
                buffer,
                sizeof(buffer),
                0
            );

            if (m <= 0)
                break;

            body.append(buffer, m);
        }

        handleCommand(body);

        sendResponse(
            client,
            200,
            "application/json; charset=utf-8",
            stateJson()
        );

        return;
    }

    sendResponse(
        client,
        404,
        "text/plain",
        "Not found"
    );
}

// ==========================================
// MAIN FUNCTION
// ==========================================

int main() {

    cout << "=====================================\n";
    cout << " SMART CITY EMERGENCY COMMAND CENTER\n";
    cout << "=====================================\n\n";

    // Load previous data

    if (!loadData(false)) {

        resultText =
            "Backend ready. Default data loaded.";
    }

    // Initialize Winsock

    WSADATA wsa;

    if (WSAStartup(
        MAKEWORD(2, 2),
        &wsa
    ) != 0) {

        cout << "Winsock initialization failed.\n";

        return 1;
    }

    // Create server socket

    SOCKET server = socket(
        AF_INET,
        SOCK_STREAM,
        IPPROTO_TCP
    );

    if (server == INVALID_SOCKET) {

        cout << "Socket creation failed.\n";

        WSACleanup();

        return 1;
    }

    // Allow port reuse

    BOOL reuse = TRUE;

    setsockopt(
        server,
        SOL_SOCKET,
        SO_REUSEADDR,
        (const char*)&reuse,
        sizeof(reuse)
    );

    // Server address

    sockaddr_in address{};

    address.sin_family = AF_INET;

    address.sin_addr.s_addr =
        htonl(INADDR_LOOPBACK);

    address.sin_port =
        htons(PORT);

    // Bind

    if (bind(
        server,
        (sockaddr*)&address,
        sizeof(address)
    ) == SOCKET_ERROR) {

        cout << "Port 8080 is already in use.\n";

        closesocket(server);

        WSACleanup();

        return 1;
    }

    // Listen

    if (listen(server, 10) == SOCKET_ERROR) {

        cout << "Server listen failed.\n";

        closesocket(server);

        WSACleanup();

        return 1;
    }

    cout << "Server started successfully!\n";

    cout << "Dashboard URL:\n";

    cout << "http://127.0.0.1:8080/\n\n";

    cout << "Keep this terminal open.\n";

    cout << "Press CTRL + C to stop server.\n";

    // Open dashboard

    ShellExecuteA(
        NULL,
        "open",
        "http://127.0.0.1:8080/",
        NULL,
        NULL,
        SW_SHOWNORMAL
    );

    // SERVER LOOP

    while (true) {

        fd_set readfds;

        FD_ZERO(&readfds);

        FD_SET(server, &readfds);

        timeval timeout{};

        timeout.tv_sec = 0;

        timeout.tv_usec = 500000;

        int ready = select(
            0,
            &readfds,
            NULL,
            NULL,
            &timeout
        );

        if (ready > 0 &&
            FD_ISSET(server, &readfds)) {

            SOCKET client = accept(
                server,
                NULL,
                NULL
            );

            if (client != INVALID_SOCKET) {

                serveClient(client);

                closesocket(client);
            }
        }

        // AUTOMATIC TRAFFIC SIMULATION

        if (trafficSimulation) {

            DWORD now = GetTickCount();

            if (now - lastTrafficTick >= 3000) {

                updateTraffic();

                lastTrafficTick = now;
            }
        }
    }

    closesocket(server);

    WSACleanup();

    return 0;
}
