#!/usr/bin/python3
import matplotlib.pyplot as plt
import numpy as np
import math
import json
import cv2
import sys
import os

with open("simdata.json", "r") as DATA:
    data = json.load(DATA)

THETA = 120
CEILING_HEIGHT = 5.51

os.environ['DISPLAY'] = "10.1.32.222:0.0"

cap = cv2.VideoCapture("0000-0499.mkv")
if not cap.isOpened():
    print("Could not open video")
    sys.exit(-1)

fps = cap.get(cv2.CAP_PROP_FPS)
print(f"FPS: {fps}")

position = [15, 0, 0]
velocity = [0, 0, 0]
atlas = {}

def camera_to_relative(u, v, width, height, theta, phi, ceiling_height):
    """
    u and v are the coordinates of the point in pixels with 0,0 at the top left
    width and height are the dimensions of the source image
    theta and phi are the field of view in degrees for width and height respectively
    returns x, y where x and y are in meters relative to the center of the image
    """

    cu = u - width/2
    cv = height/2 - v
    theta = math.pi * theta / 180
    phi = math.pi * phi / 180
    scale_x = ceiling_height * (math.sin(theta/2)/math.sin(math.pi/2 - (theta/2)))
    scale_y = ceiling_height * (math.sin(phi/2)/math.sin(math.pi/2 - (phi/2)))
    x = (cu/(width/2))*scale_x
    y = (cv/(width/2))*scale_x
    return x, y

    hangle = (theta/width)*cu
    vangle = (phi/height)*cv
    x = ceiling_height * (math.sin(hangle)/math.sin(math.pi/2 - hangle))
    y = ceiling_height * (math.sin(vangle)/math.sin(math.pi/2 - vangle))
    return x, y

def relative_to_absolute(point, position):
    x, y, theta = position
    r = math.sqrt(point[0]**2 + point[1]**2)
    rtheta = math.atan2(point[1], point[0])
    px = r*math.cos(rtheta-theta) + x
    py = r*math.sin(rtheta-theta) + y
    return px, py

def match_point(point, atlas, cutoff=1):
    nearest_point = None
    nearest_distance = None
    for label, known_point in atlas.items():
        distance = math.sqrt((point[0]-known_point[0])**2 + (point[1]-known_point[1])**2)
        if distance > cutoff:
            continue
        if nearest_point is None:
            nearest_point = label
            nearest_distance = distance
        elif nearest_distance > distance:
            nearest_point = label
            nearest_distance = distance
    return nearest_point

def update_position(position, matches, atlas):
    new_position = list(position)
    dx = 0
    dy = 0
    for point, match in matches:
        point, match = matches[0]
        previous_point = atlas[match]
        dx += previous_point[0] - point[0]
        dy += previous_point[1] - point[1]
    if matches:
        new_position[0] = position[0] + (dx / len(matches))
        new_position[1] = position[1] + (dy / len(matches))
    return new_position

def trusted_point(point, width, height):
    border = 100
    if point[0] < border:
        return False
    if width - point[0] < border:
        return False
    if point[1] < border:
        return False
    if height - point[1] < border:
        return False
    return True

if __name__ == "__main__":
    pos_x = []
    pos_y = []
    saved = False
    idx = 0
    while cap.isOpened():
        ret, frame = cap.read()
        if ret:
            # Set our filtering parameters
            # Initialize parameter settiing using cv2.SimpleBlobDetector
            params = cv2.SimpleBlobDetector_Params()
            
            # Set Area filtering parameters
            params.filterByArea = True
            params.minArea = 1
            params.maxArea = 50000
            
            # Set Circularity filtering parameters
            params.filterByCircularity = True
            params.minCircularity = 0.8
            
            # Set Convexity filtering parameters
            params.filterByConvexity = False
            params.minConvexity = 0.2
                
            # Set inertia filtering parameters
            params.filterByInertia = False
            params.minInertiaRatio = 0.01

            params.minThreshold = 128
            params.maxThreshold = 255

            params.filterByColor = False
            params.blobColor = 255
            
            # Create a detector with the parameters
            detector = cv2.SimpleBlobDetector_create(params)
                
            # Detect blobs
            keypoints = detector.detect(frame)
            height, width = frame.shape[:2]
            phi = (THETA/width)*height
            matches = []
            new_points = []
            likely_pos = [position[0]+velocity[0], position[1]+velocity[1], position[2]+velocity[2]]
            for keypoint in keypoints:
                u, v = keypoint.pt
                relative_point = camera_to_relative(u, v, width, height, THETA, phi, CEILING_HEIGHT)
                likely_point = relative_to_absolute(relative_point, likely_pos)
                point = relative_to_absolute(relative_point, position)
                match = match_point(likely_point, atlas)
                if match:
                    matches.append((point, match))
                else:
                    if trusted_point((u, v), width, height):
                        new_points.append(relative_point)
            new_position = update_position(position, matches, atlas)
            velocity = [new_position[0]-position[0], new_position[1]-position[1], new_position[2]-position[2]]
            #print(velocity)
            position = new_position
            print(position, len(keypoints), len(matches), len(atlas))
            data[idx].update({
                "estimated_position": position,
                "visible_points": len(keypoints),
                "matched_points": len(matches),
                "mapped_points": len(atlas)
            })
            idx += 1
            pos_x.append(position[0])
            pos_y.append(position[1])
            for new_point in new_points:
                atlas[f"Point {len(atlas):03d}"] = relative_to_absolute(new_point, position)
            #print(position, len(keypoints), len(matches), len(atlas))

            if not saved:
                cv2.imwrite("noblobs.jpg", frame)
                saved = True
            blobs = cv2.drawKeypoints(frame, keypoints, frame, (0, 0, 255), cv2.DRAW_MATCHES_FLAGS_DRAW_RICH_KEYPOINTS)
            resized = cv2.resize(frame, (frame.shape[1] // 4, frame.shape[0] // 4), 0.25, 0.25, cv2.INTER_NEAREST)
            
            cv2.imshow('Frame', resized)
            key = cv2.waitKey(1)
            if key == ord('q'):
                break
        else:
            break
    cap.release()
    cv2.destroyAllWindows()
#print(json.dumps(atlas, indent=2, sort_keys=True))
x = []
y = []
for point in atlas.values():
    x.append(point[0])
    y.append(point[1])

fig = plt.figure()
ax = fig.add_subplot(111)
ax.set_aspect('equal')
plt.scatter(x, y, color="blue")
plt.scatter(pos_x, pos_y, color="red", zorder=1)
plt.plot(pos_x, pos_y, zorder=2)
plt.savefig("atlas.png", dpi=300, bbox_inches="tight")
print(len(pos_x))

with open("full_simdata.json", "w") as DATA:
    json.dump(data, DATA, indent=2)