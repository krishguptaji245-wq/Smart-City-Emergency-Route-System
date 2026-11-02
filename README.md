# Smart City Emergency Route System

A mini project demonstrating how Data Structures and Algorithms can
support emergency route planning and priority-based incident handling in
a smart-city environment.

## Team Members

  Name             Roll No.
  -------------- ----------
  Krish Gupta            26
  Vishav Dutta           22
  Smarat Ahire            4

**Project Guide:** Ms. P. D. Rakibe\
**Institute:** K. K. Wagh Institute of Engineering Education & Research,
Nashik\
**Department:** Computer Engineering

## Project Overview

The Smart City Emergency Route System combines a C++ backend with a
browser-based dashboard. It demonstrates a simplified city graph,
shortest-path calculation, emergency prioritisation, traffic-state
updates, and map-based route display.

Users can search for source and destination locations in Nashik and view
a road route with distance and estimated travel time. Emergency requests
can be added and processed according to priority.

## Main Features

-   Interactive map with location search.
-   Road route display with distance and estimated duration.
-   C++ backend using a local HTTP server.
-   Weighted graph containing seven predefined project locations.
-   Dijkstra's algorithm for shortest-path calculation on the predefined
    graph.
-   Max-Heap concept for processing higher-priority emergencies first.
-   Traffic states: Low, Moderate, Heavy, and Closed.
-   Emergency management and traffic simulation.
-   Save and load application state.

## Technologies Used

-   **C++17** -- backend logic and data structures.
-   **WinSock** -- local HTTP server on Windows.
-   **HTML, CSS, JavaScript** -- dashboard interface.
-   **Leaflet** -- interactive map.
-   **OpenStreetMap** -- map tiles.
-   **Photon** -- place search/geocoding.
-   **OSRM** -- street-level road routing.

## Project Structure

``` text
Smart-City-Emergency-Route-System/
├── main.cpp
├── index.html
├── README.md
└── smartcity_data.txt   # created/used by the application for saved state
```

## Requirements

-   Windows operating system.
-   MinGW-w64 / g++ with C++17 support.
-   Internet connection for map tiles, place search, and road routing.

## Compile and Run

Open a terminal in the project folder and compile:

``` bash
g++ -std=c++17 main.cpp -o SmartCity.exe -lws2_32 -lshell32
```

Run the server:

``` powershell
.\SmartCity.exe
```

Then open this address in your browser:

``` text
http://127.0.0.1:8080/
```

Keep the C++ server running while using the dashboard. Do not use VS
Code Live Server for this version; open the local server address above.

## Data Structures and Algorithms

### Weighted Graph

The backend uses seven predefined vertices:

-   Central
-   City Hospital
-   Police Station
-   Fire Station
-   Airport
-   Market
-   North Junction

Road connections are represented as weighted edges.

### Dijkstra's Algorithm

Dijkstra's algorithm calculates a minimum-cost path through the
predefined project graph. It is used to demonstrate shortest-path
concepts on the configured graph.

### Max-Heap

A Max-Heap is used for priority-based emergency handling. The
highest-priority pending emergency is selected first.

## Important Route Calculation Note

The street-level route drawn between searched map locations is requested
from the OSRM routing service. The C++ Dijkstra implementation works on
the project's predefined seven-location graph. These are separate route
calculations; the project does not currently run Dijkstra over every
street in Nashik.

Photon and OSRM are external services and require internet access. Their
availability and results may vary.

## Testing Checklist

-   [ ] Start the C++ server and open the dashboard.
-   [ ] Search for two locations in Nashik.
-   [ ] Display the road route, distance, and estimated duration.
-   [ ] Add emergency requests with different priorities.
-   [ ] Process the next emergency and verify priority ordering.
-   [ ] Change traffic states and test simulation.
-   [ ] Test saving and loading application state.

## Limitations and Future Scope

The backend graph is a simplified model with seven predefined locations.
The map route depends on external online services. The current prototype
is not connected to official emergency dispatch, live vehicle GPS, or an
official live traffic-control system.

Future improvements may include a larger real-road graph, live traffic
feeds, GPS-enabled emergency vehicles, user authentication, and
database-backed incident records.

## Academic Use

This repository is an educational mini project for demonstrating graph
representation, Dijkstra's algorithm, Max-Heap priority handling,
traffic-state modelling, and communication between a C++ backend and a
web dashboard.
