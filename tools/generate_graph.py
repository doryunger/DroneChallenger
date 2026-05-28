"""
Generates nodes.csv and edges.csv for the Munich playground (2km radius
around Marienplatz) from the OpenStreetMap road network via Overpass API.

Every consecutive node pair in each OSM way becomes an edge, so the car
follows road curvature closely rather than cutting corners between
intersections. IDs are remapped to sequential integers starting at 1 so
they never exceed int32.

Usage:
    pip install requests
    python generate_graph.py
    Copy the output nodes.csv and edges.csv to Content/Graph/
"""

import csv
import math
import requests

CENTER_LAT =  48.1374
CENTER_LON =  11.5755
RADIUS_M   =  2000

HIGHWAY_FILTER = (
    "motorway|trunk|primary|secondary|tertiary|"
    "unclassified|residential|living_street|service"
)

OVERPASS_URL = "https://overpass-api.de/api/interpreter"


def fetch_osm() -> dict:
    query = f"""
[out:json][timeout:90];
(
  way["highway"~"^({HIGHWAY_FILTER})$"]
     (around:{RADIUS_M},{CENTER_LAT},{CENTER_LON});
);
out body;
>;
out skel qt;
"""
    print("Querying Overpass API …")
    r = requests.post(OVERPASS_URL, data={"data": query}, timeout=120)
    r.raise_for_status()
    return r.json()


def haversine(lat1: float, lon1: float, lat2: float, lon2: float) -> float:
    R = 6_371_000.0
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dp = math.radians(lat2 - lat1)
    dl = math.radians(lon2 - lon1)
    a  = math.sin(dp / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dl / 2) ** 2
    return 2 * R * math.asin(math.sqrt(a))


def dist_from_center(lat: float, lon: float) -> float:
    return haversine(CENTER_LAT, CENTER_LON, lat, lon)


def main() -> None:
    data = fetch_osm()

    raw_nodes: dict[int, tuple[float, float]] = {}
    for el in data["elements"]:
        if el["type"] == "node":
            raw_nodes[el["id"]] = (el["lat"], el["lon"])

    raw_edges: list[tuple[int, int, float]] = []
    used_node_ids: set[int] = set()

    for el in data["elements"]:
        if el["type"] != "way" or "nodes" not in el:
            continue
        way_nodes = el["nodes"]
        for i in range(len(way_nodes) - 1):
            a, b = way_nodes[i], way_nodes[i + 1]
            if a not in raw_nodes or b not in raw_nodes:
                continue
            lat_a, lon_a = raw_nodes[a]
            lat_b, lon_b = raw_nodes[b]
            if dist_from_center(lat_a, lon_a) > RADIUS_M and dist_from_center(lat_b, lon_b) > RADIUS_M:
                continue
            length = haversine(lat_a, lon_a, lat_b, lon_b)
            raw_edges.append((a, b, length))
            raw_edges.append((b, a, length))
            used_node_ids.add(a)
            used_node_ids.add(b)

    id_map: dict[int, int] = {
        osm_id: seq_id
        for seq_id, osm_id in enumerate(sorted(used_node_ids), start=1)
    }

    with open("nodes.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["node_id", "lon", "lat"])
        for osm_id in sorted(used_node_ids):
            lat, lon = raw_nodes[osm_id]
            w.writerow([id_map[osm_id], f"{lon:.7f}", f"{lat:.7f}"])

    with open("edges.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["from", "to", "length"])
        for a, b, length in raw_edges:
            w.writerow([id_map[a], id_map[b], f"{length:.3f}"])

    print(f"nodes.csv : {len(used_node_ids)} nodes")
    print(f"edges.csv : {len(raw_edges)} directed edges")
    print("Copy both files to Content/Graph/ and restart PIE.")


if __name__ == "__main__":
    main()
