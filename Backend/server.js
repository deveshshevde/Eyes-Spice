const express = require('express');
const fs = require('fs');
const axios = require('axios');
const path = require('path');
const app = express();

app.use(express.static('public')); 

app.post('/upload', express.raw({ type: 'image/jpeg', limit: '5mb' }), (req, res) => {
    const targetPath = `uploads/${Date.now()}.jpg`;
    fs.writeFile(targetPath, req.body, err => {
        if (err) return res.status(500).send("Error saving image");
        console.log(`Image saved to ${targetPath}`);
        res.send("Image saved");
    });
});

app.get('/request-capture', async (req, res) => {
    try {
        const response = await axios.get('http://192.168.1.16/capture-request');  
        console.log("ESP32 responded:", response.data);
        res.send("Capture request sent to ESP32");
    } catch (err) {
        console.error("Failed to contact ESP32:", err.message);
        res.status(500).send("ESP32 request failed");
    }
});

app.listen(3000, () => {
    console.log("Server running at http://localhost:3000");
});