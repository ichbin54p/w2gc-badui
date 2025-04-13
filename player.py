import cv2
import pygame
from pygame import mixer
import numpy as np
from moviepy import VideoFileClip
import time, os
import threading


paused = False
pygame.init()
mixer.init()

video_path = 'video.mp4'
os.system('./FFmpeg -i ./'+video_path+' ./audio.mp3')

cap = cv2.VideoCapture(video_path, cv2.CAP_FFMPEG)
fps = cap.get(cv2.CAP_PROP_FPS)
window = "W2G"
interframe_wait_ms = 30
frame_time = 1000 / fps
frame_count = cap.get(cv2.CAP_PROP_FRAME_COUNT)
duration = frame_count/fps
click_seek = -1

start_time = time.time()

if not cap.isOpened():
    
    print("Error: Could not open video.")
    exit()
       
cv2.namedWindow(window, cv2.WND_PROP_FULLSCREEN)
cv2.setWindowProperty(window, cv2.WND_PROP_FULLSCREEN, cv2.WINDOW_FULLSCREEN)
mixer.music.load('audio.mp3')
def play_audio():
    pygame.mixer.music.play()
audio_thread = threading.Thread(target=play_audio)
audio_thread.start()

def mouse_callback(event, x, y, flags, param):
    global click_seek, width, height
    if width == 0 or height == 0:
        return
    
    if event == cv2.EVENT_LBUTTONDOWN:
        if height - bar_height <= y < height:
            
            percent = x / width
            click_seek = int(frame_count * percent)
    print(f"Clicked at x={x}, y={y} — seeking to frame {click_seek}")
            
cv2.setMouseCallback(window, mouse_callback)            
            
while cap.isOpened():  
    
    if click_seek >= 0:
        cap.set(cv2.CAP_PROP_POS_FRAMES, click_seek)
        cap.grab()
        ret, frame = cap.read()
        click_seek = -1
    
    if not paused:
        ret, frame = cap.read()
        if not ret:            
            break
         
        elapsed = time.time() - start_time
        expected_frame = int(elapsed * fps)
        current_frame = int(cap.get(cv2.CAP_PROP_POS_FRAMES))

        if current_frame < expected_frame:
            continue

        cv2.imshow(window, frame)
    else:
        cv2.imshow(window, frame)

    key = cv2.waitKey(int(frame_time)) & 0xFF
    
    cv2.rectangle(frame, (0, 1919), (1079, 1919), (0,255,0), 2)
    cv2.imshow(window, frame)
    
    height, width = frame.shape[:2]
    
    bar_height = 20
    bary = height - bar_height
    cv2.rectangle(frame, (0, bary), (width, height), (50, 50, 50), -1)
    click_seek = -1
    
    progress = int((current_frame / frame_count) * width)
    cv2.rectangle(frame, (0, bary), (progress, height), (0, 255, 0), -1)
    
    current_time_progress = current_frame / fps
    total_time = frame_count / fps
    time_text = f"{current_time_progress:.1f}s / {total_time:.1f}s"
    cv2.putText(frame, time_text, (10, bary - 10), cv2.FONT_HERSHEY_COMPLEX, 0.5, (123, 0, 165), 1)
         
    last_seek = 0
    seek_delay = 0.3
    
    if time.time() - last_seek > seek_delay:
        last_seek = time.time()
        
    if key == ord('q'):
        break
    elif key == ord(' '):
        paused = not paused
        if paused:
            pygame.mixer.music.pause()
        else:
            pygame.mixer.music.unpause()
            start_time = time.time() - (cap.get(cv2.CAP_PROP_POS_MSEC) / 1000)
    elif key == ord('w'):
        vol = pygame.mixer.music.get_volume()
        vol = min(1.0, vol + 0.05)
        pygame.mixer.music.set_volume(vol)

    elif key == ord('s'):
        vol = pygame.mixer.music.get_volume()
        vol = max(0.0, vol - 0.05)
        pygame.mixer.music.set_volume(vol)
    elif key == ord('d'):
        current_time = int(current_frame/fps)
        skipped_time = int(current_time + 5)
        pygame.mixer.music.stop()
                
        ret, frame = cap.read()
        if ret:
            cv2.imshow(window, frame)
        
        time.sleep(0.5)
        
        pygame.mixer.music.play(start=skipped_time)
        cap.set(cv2.CAP_PROP_POS_FRAMES, current_time + int(fps * 5))
    elif key == ord('a'):
        currentback_time = int(current_frame/fps)
        skippedback_time = int(current_time - 5)
        cap.set(cv2.CAP_PROP_POS_FRAMES, max(0, currentback_time - int(fps *5)))
        pygame.mixer.music.stop()
        ret, frame = cap.read()
        if ret:
            cv2.imshow(window, frame)
            
        time.sleep(0.5)
               
        pygame.mixer.music.play(start=skippedback_time)
        
cap.release()
cv2.destroyAllWindows()
pygame.mixer.music.stop()