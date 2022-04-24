#!/usr/bin/env python3

from tkinter import *
from tkinter.ttk import *
import paho.mqtt.client as mqtt
import ssl

class myApplication:

    def __init__(self):
        # Get paho connected
        self.client = mqtt.Client( client_id="mqttify-gui", userdata=None, 
                              protocol=mqtt.MQTTv311)
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message

        # TLS is mandatory for HiveMQ
        self.client.tls_set(tls_version=ssl.PROTOCOL_TLS)
        self.client.connect("ec36fe04c68947d399f3cbbc782e89ff.s2.eu.hivemq.cloud", 
                              8883, 60)
        # TODO: get these from a file instead?
        with open("../passwd.txt") as file:
            self.client.username_pw_set(
                    file.readline().rstrip(' \n\r\t'),
                    file.readline().rstrip(' \n\r\t')
                    )

        # start threaded mainloop
        self.client.loop_start()

        # Root window settings
        self.root = Tk()
        self.root.geometry("550x600")
        self.root.minsize(500,600)
        self.root.maxsize(1080,720)
        self.root.resizable(False, False)
        self.root.title("MQTTify")
        self.subPanel = SubscribePanel(self.root, self.client)
        self.pubPanel = PublishPanel(self.root, self.client)
        # Create frames
        self.root.mainloop()


    def on_connect(self, client, userdata, flags, rc):
        print("Connection returned result: "+mqtt.connack_string(rc))
        

    def on_disconnect(self, client, userdata, rc):
        if rc != 0:
            print("Unexpected disconnection.")

    def on_message(self, client, userdata, message):
        msg = message.payload.decode('utf-8', "ignore")
        #msg = str(message.payload).encode('ascii', "ignore").decode("ascii")
        #msg = msg.decode()
        #msg = msg.decode('ascii')
        print('message: ' + msg)
        self.subPanel.set_message_text(msg)

class SubscribePanel():

    def __init__(self, window, client):
        self.window = window
        self.client = client
        self.topic_text = StringVar()
        self.panel = LabelFrame(window, text='Subscribe', relief='raised', borderwidth=1)
        self.panel.pack(fill='both', expand=True, padx=10, pady=10)

        # setup the Topic frame
        self.topic_frame = LabelFrame(self.panel, text='Topic')
        self.topic_frame.pack(expand=True, padx=5, pady=5)
        self.topic_entry = Entry(self.topic_frame, textvariable=self.topic_text)
        self.topic_entry.pack(side='left', padx=5, pady=5)
        button = Button(self.topic_frame, text='Set', command=self.topic_set)
        button.pack(padx=5, pady=5)
        button = Button(self.topic_frame, text='Reset', command=self.topic_reset)
        button.pack(padx=5, pady=5)

        # setup the printing frame
        self.print_frame = LabelFrame(self.panel, text='Messages')
        self.print_frame.pack(padx=5, pady=5)
        self.message_text = Text(self.print_frame, width=80, height=10, wrap='none')
        self.message_text.bind("<Key>", lambda e: "break")
        self.message_text.pack(padx=5, pady=5)
        self.message_text_nlines = 0

        
    def topic_set(self):
        self.topic_entry.bind("<Key>", lambda e: "break")
        topic=self.topic_entry.get()
        print("Subscribing to topic %s", topic)
        self.client.subscribe(topic, qos=1)


    def topic_reset(self):
        topic=self.topic_entry.get()
        self.client.unsubscribe(topic)
        print("Unsubscribing from topic %s", topic)
        self.clear_message_text()
        self.topic_entry.delete(0,END)
        self.topic_entry.unbind("<Key>")

    def set_message_text(self, text):
        #if self.message_text_nlines > 10:
        #    self.message_text.delete("10.0", "end")
        #else:
        #    self.message_text_nlines+=1
        #self.message_text.insert(1.0, text)
        #self.message_text.insert("1.0", text.rstrip(' \t\n\r'))
        nlines = int(self.message_text.index('end - 1 line').split('.')[0])
        if self.message_text_nlines == 10:
            self.message_text.delete(1.0, 2.0)
        #if self.message_text.index('end-1c')!='1.0':
        #    self.message_text.insert('end', '\n')
        self.message_text.insert('end', text)

    def clear_message_text(self):
        self.message_text.delete("1.0", "end")


class PublishPanel():

    def __init__(self, window, client):
        self.window = window
        self.client = client
        self.topic_text = StringVar()
        self.panel = LabelFrame(self.window, text='Publish', relief='raised', borderwidth=1)
        self.panel.pack(fill='both', expand=True, padx=10, pady=10)

        # setup the Topic frame
        self.topic_frame = LabelFrame(self.panel, text='Topic')
        self.topic_frame.pack(expand=True, padx=5, pady=5)
        self.topic_entry = Entry(self.topic_frame, textvariable=self.topic_text)
        self.topic_entry.pack(side='left', padx=5, pady=5)
        button = Button(self.topic_frame, text='Set', command=self.topic_set)
        button.pack(padx=5, pady=5)
        button = Button(self.topic_frame, text='Reset', command=self.topic_reset)
        button.pack(padx=5, pady=5)

        # setup the publish entry 
        self.publish_text = StringVar()
        self.pub_entry = Entry(self.panel, width=80, textvariable=self.publish_text)
        self.pub_entry.pack(padx=5, pady=5)
        button = Button(self.panel, text='Publish', command=self.publish)
        button.pack(padx=5, pady=5)

    def topic_set(self):
        self.topic_entry.bind("<Key>", lambda e: "break")

    def topic_reset(self):
        self.topic_entry.delete(0,END)
        self.topic_entry.unbind("<Key>")
        print("Topic reset")
        
    def publish(self):
        # publish to broker
        topic =self.topic_entry.get()
        msg = self.pub_entry.get()
        msg = msg.strip('\r\n\t')
        msg += '\n'
        if msg is not None:
            print("publishing", topic, msg)
            self.client.publish(topic, payload=msg, qos=1)
            self.pub_entry.delete(0,END)


def main():
    app = myApplication()

if __name__ == "__main__":
    main()
