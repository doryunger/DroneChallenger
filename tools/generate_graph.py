"""
Generates nodes.csv, edges.csv, and munich_graph.js for the Munich playground
(2km radius around Marienplatz) from the OpenStreetMap road network via
Overpass API, with elevation data fetched from the Open Elevation API.

Every consecutive node pair in each OSM way becomes an edge. IDs are remapped
to sequential integers starting at 1.

munich_graph.js uses Unreal world-space coordinates:
  x = northing offset from centre in cm  (positive = North)
  y = easting  offset from centre in cm  (positive = East)
  z = elevation above sea level   in cm

Usage:
    pip install requests
    python generate_graph.py
    Copy nodes.csv and edges.csv to Content/Graph/
    Copy munich_graph.js to studio/
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

OVERPASS_URLS = [
    "https://overpass-api.de/api/interpreter",
    "https://overpass.kumi.systems/api/interpreter",
    "https://maps.mail.ru/osm/tools/overpass/api/interpreter",
]
OPEN_ELEVATION_URL  = "https://api.open-elevation.com/api/v1/lookup"
ELEVATION_BATCH     = 512


def fetch_osm() -> dict:
    query = f"""
[out:json][timeout:120];
(
  way["highway"~"^({HIGHWAY_FILTER})$"]
     (around:{RADIUS_M},{CENTER_LAT},{CENTER_LON});
);
out body;
>;
out skel qt;
"""
    headers = {"Accept": "application/json", "User-Agent": "DroneChallenger/1.0"}
    for url in OVERPASS_URLS:
        print(f"Querying Overpass API ({url}) …")
        try:
            r = requests.post(url, data={"data": query}, headers=headers, timeout=150)
            r.raise_for_status()
            return r.json()
        except Exception as exc:
            print(f"  failed: {exc} — trying next mirror …")
    raise RuntimeError("All Overpass mirrors failed")


def fetch_elevations(node_latlon: dict[int, tuple[float, float]]) -> dict[int, float]:
    items  = list(node_latlon.items())
    result: dict[int, float] = {}
    total  = len(items)
    for i in range(0, total, ELEVATION_BATCH):
        batch     = items[i:i + ELEVATION_BATCH]
        locations = [{"latitude": lat, "longitude": lon} for _, (lat, lon) in batch]
        hi        = min(i + ELEVATION_BATCH, total)
        print(f"Fetching elevations {i + 1}–{hi} of {total} …")
        r = requests.post(OPEN_ELEVATION_URL, json={"locations": locations}, timeout=120)
        r.raise_for_status()
        for (osm_id, _), rec in zip(batch, r.json()["results"]):
            result[osm_id] = float(rec["elevation"])
    return result


def haversine(lat1: float, lon1: float, lat2: float, lon2: float) -> float:
    R  = 6_371_000.0
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dp = math.radians(lat2 - lat1)
    dl = math.radians(lon2 - lon1)
    a  = math.sin(dp / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dl / 2) ** 2
    return 2 * R * math.asin(math.sqrt(a))


def dist_from_center(lat: float, lon: float) -> float:
    return haversine(CENTER_LAT, CENTER_LON, lat, lon)


def latlon_to_world_cm(lat: float, lon: float) -> tuple[int, int]:
    dlat  = lat - CENTER_LAT
    dlon  = lon - CENTER_LON
    x_cm  = int(round(dlat * 111_320 * 100))
    y_cm  = int(round(dlon * 111_320 * math.cos(math.radians(CENTER_LAT)) * 100))
    return x_cm, y_cm


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
            if (dist_from_center(lat_a, lon_a) > RADIUS_M and
                    dist_from_center(lat_b, lon_b) > RADIUS_M):
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

    elevations = fetch_elevations({osm_id: raw_nodes[osm_id] for osm_id in used_node_ids})

    with open("nodes.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["node_id", "lon", "lat", "elevation"])
        for osm_id in sorted(used_node_ids):
            lat, lon  = raw_nodes[osm_id]
            elev      = elevations.get(osm_id, 520.0)
            w.writerow([id_map[osm_id], f"{lon:.7f}", f"{lat:.7f}", f"{elev:.1f}"])

    with open("edges.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["from", "to", "length"])
        for a, b, length in raw_edges:
            w.writerow([id_map[a], id_map[b], f"{length:.3f}"])

    with open("munich_graph.js", "w", encoding="utf-8") as f:
        node_parts: list[str] = []
        for osm_id in sorted(used_node_ids):
            lat, lon       = raw_nodes[osm_id]
            elev           = elevations.get(osm_id, 520.0)
            x_cm, y_cm     = latlon_to_world_cm(lat, lon)
            z_cm           = int(round(elev * 100))
            seq_id         = id_map[osm_id]
            node_parts.append(f"{seq_id}:[{x_cm},{y_cm},{z_cm}]")
        f.write("const GRAPH_NODES = {" + ",".join(node_parts) + "};\n")

        seen_edges: set[tuple[int, int]] = set()
        edge_parts: list[str] = []
        for a, b, _ in raw_edges:
            sa, sb = id_map[a], id_map[b]
            key    = (min(sa, sb), max(sa, sb))
            if key not in seen_edges:
                seen_edges.add(key)
                edge_parts.append(f"[{sa},{sb}]")
        f.write("const GRAPH_EDGES = [" + ",".join(edge_parts) + "];\n")

    print(f"nodes.csv    : {len(used_node_ids)} nodes")
    print(f"edges.csv    : {len(raw_edges)} directed edges")
    print(f"munich_graph.js: {len(used_node_ids)} nodes, {len(seen_edges)} undirected edges")
    print("Copy nodes.csv and edges.csv to Content/Graph/")
    print("Copy munich_graph.js to studio/")


if __name__ == "__main__":
    main()
