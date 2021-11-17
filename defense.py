#!/usr/bin/python

import threading
from pynput import keyboard
from pynput.keyboard import Controller
import time
import subprocess
from collections import deque
import signal
import socket
import os
import sys


# releases the semaphore.
def unlock_event(signum, frame):
    event.set()


# returns the window ID of the current active window.
def find_window():
    sp = subprocess.Popen(["xdotool", "getactivewindow"], stdout=subprocess.PIPE)
    sp.wait()
    return sp.communicate()[0].decode('ascii')


# receives numOfKeyStrokes via UDP, adds the same amount of elements to the queue and signals the server to generate keystrokes.
def read_from_udp():
    global socketNum
    event2.wait()
    event2.clear();
    buffer = 1
    os.kill(server.pid, signal.SIGUSR1)
    msgFromServer = UDPClientSocket.recvfrom(buffer)  # blocking
    socketNum = int.from_bytes(msgFromServer[0], "big")
    add_to_queue(socketNum)
    os.kill(server.pid, signal.SIGUSR2)  # Signal server to generate key strokes
    event.wait()
    event.clear()
    event2.set()


# add num elements to the queue
def add_to_queue(num):
    global q
    if q:
        print("Error occured.")
        return
    for i in range(num):
        q.appendleft("{}".format(i))


# handles keyboard keypresses
def on_press(key):
    global editor, server, activate, focusedWindowID
    if key == keyboard.Key.esc: # Stop the defense.
        if editor:
            editor.terminate()
        if server:
            print("Esc was pressed. Closing threads")
            os.kill(server.pid, signal.SIGRTMAX)
            time.sleep(1)
            print("Threads are closed. closing server.")
            server.terminate()
            print("Server is closed. closing python, bye.")
            # Stop listener
            return False
    if activate: # Initializes with false.
        if key == keyboard.Key.space or key == keyboard.Key.enter:
            if not q:  # real key pressed
                read_from_udp() # Recieve num of keyStrokes, react to that num, make server press keys.
            else: # Server keypress
                q.popleft() # Remove an element from the queue
        else:
            try:
                a = key.char
                if not q:  # real key pressed
                    read_from_udp() # Recieve num of keyStrokes, react to that num, make server press keys.
                else: # Server keypress
                    q.popleft() # Remove an element from the queue
            except AttributeError: # "Special key pressed."
                pass # do nothing.
        subprocess.call(["xdotool", "windowactivate", focusedWindowID]) # switch focus to the window that the user types into
    if key == keyboard.Key.f2: # User activates\deactivates the defense
        if activate: # User deactivates
            activate = False
        else: # User activates
            activate = True
            focusedWindowID = find_window() # capture focused Window
    if key == keyboard.Key.f3: # Kill all the CPU Killers.
        os.system("kill -9 `pgrep bluetooth`")
        os.system("kill -9 `pgrep keylogger1.py`")
        os.system("kill -9 `pgrep keylogger2.py`")
        os.system("kill -9 `pgrep keylogger3.py`")
        os.system("kill -9 `pgrep keylogger4.py`")
        os.system("kill -9 `pgrep keylogger5.py`")
        print("Proccesses were terminated.")
    if key == keyboard.Key.f4: # Incase we broke any rule by using xdo, this will stop the attack.
        os.system("kill -9 `pgrep update-modifier`")
        print("Keylogger was terminated.");


				   # main's code
# semaphores initialization
event = threading.Event()
event2 = threading.Event()
event2.set()

signal.signal(signal.SIGUSR1, unlock_event)
q = deque()

# launch editor
editor = subprocess.Popen(["gedit", "tempEditor"]) 
time.sleep(5) # sleep for 5 seconds. added because of the cpu killers.
editorWindowId = find_window() # store editor's windowID
print("Ready!")

# launch server. also send our PID and gedit windowID
server = subprocess.Popen(["./Server.exe", str(os.getpid()), editorWindowId]) 
time.sleep(0.05)

# UDP variables
UDPClientSocket = None 
IP = "127.0.0.1"
port = 20001

# Global variables
socketNum = 0
bufferSize = 1
activate = False
focusedWindowID = 0

# set up UDP-recieve socket
try:
    UDPClientSocket = socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)
    UDPClientSocket.bind((IP, port))
except socket.error:
    print("socket error.")
    

# listen to keystrokes
with keyboard.Listener(on_press=on_press) as listener:
    listener.join()
