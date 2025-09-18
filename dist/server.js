import mqtt from 'mqtt';
import { MongoClient } from 'mongodb';
import dotenv from 'dotenv';
import express from 'express';
// Load environment variables
dotenv.config();
class ESP32MQTTServer {
    mqttClient;
    mongoClient;
    db;
    collection;
    messageCount = 0;
    app;
    constructor() {
        const mongoUrl = process.env.MONGO_CONNECTION_STRING || 'mongodb://localhost:27017';
        this.mqttClient = mqtt.connect(process.env.MQTT_HOST, {
            username: process.env.MQTT_USERNAME,
            password: process.env.MQTT_PASSWORD,
            rejectUnauthorized: process.env.NODE_ENV === 'production'
        });
        this.mongoClient = new MongoClient(mongoUrl);
        this.app = express();
        this.setupExpress();
    }
    async start() {
        try {
            await this.mongoClient.connect();
            console.log('✅ Connected to MongoDB');
            this.db = this.mongoClient.db(process.env.MONGO_DATABASE || 'dadosClima');
            this.collection = this.db.collection('clima');
            // Create indexes for performance with 1000 ESP32s
            await this.collection.createIndex({ uuid: 1, unixtime: -1 });
            await this.collection.createIndex({ unixtime: -1 });
            this.setupMQTT();
            this.startHttpServer();
        }
        catch (error) {
            console.error('❌ Failed to start server:', error);
            process.exit(1);
        }
    }
    setupExpress() {
        this.app.use(express.json());
        // Health check endpoint for CI/CD
        this.app.get('/health', (req, res) => {
            res.status(200).json({
                status: 'healthy',
                timestamp: new Date().toISOString(),
                mqtt: this.mqttClient.connected ? 'connected' : 'disconnected',
                uptime: process.uptime()
            });
        });
        // Basic stats endpoint
        this.app.get('/stats', (req, res) => {
            res.json({
                messagesProcessed: this.messageCount,
                mqttConnected: this.mqttClient.connected,
                uptime: process.uptime(),
                timestamp: new Date().toISOString()
            });
        });
    }
    startHttpServer() {
        const port = process.env.PORT || 3000;
        this.app.listen(port, () => {
            console.log(`🌐 HTTP server running on port ${port}`);
            console.log(`🩺 Health check: http://localhost:${port}/health`);
        });
    }
    setupMQTT() {
        this.mqttClient.on('connect', () => {
            console.log('🔗 Connected to MQTT broker');
            this.mqttClient.subscribe('weather/+/data', (err) => {
                if (err) {
                    console.error('❌ Failed to subscribe:', err);
                }
                else {
                    console.log('📡 Subscribed to weather/+/data');
                }
            });
        });
        this.mqttClient.on('message', async (topic, message) => {
            try {
                await this.processMessage(topic, message);
                this.messageCount++;
                if (this.messageCount % 100 === 0) {
                    console.log(`📊 Processed ${this.messageCount} messages`);
                }
            }
            catch (error) {
                console.error('❌ Error processing message:', error);
            }
        });
        this.mqttClient.on('error', (error) => {
            console.error('❌ MQTT error:', error);
        });
    }
    async processMessage(topic, message) {
        const topicParts = topic.split('/');
        if (topicParts.length !== 3 || topicParts[0] !== 'weather' || topicParts[2] !== 'data') {
            return;
        }
        try {
            // Parse JSON message - flexible for any sensor data
            const messageData = JSON.parse(message.toString());
            // Store only the ESP32 data (no server additions)
            await this.collection.insertOne(messageData);
            // Log every 10 messages for monitoring
            if (this.messageCount % 10 === 0) {
                const uuid = messageData.uuid || 'unknown';
                const sensors = Object.keys(messageData).filter(key => !['uuid', 'unixtime'].includes(key)).join(', ');
                console.log(`📊 Received from ${uuid}: ${sensors}`);
            }
        }
        catch (error) {
            console.error('❌ Failed to parse message:', error);
            console.error('Message:', message.toString());
        }
    }
    async stop() {
        this.mqttClient.end();
        await this.mongoClient.close();
        console.log('🛑 Server stopped');
    }
}
const server = new ESP32MQTTServer();
server.start().catch(console.error);
process.on('SIGINT', async () => {
    console.log('\n🔄 Shutting down server...');
    await server.stop();
    process.exit(0);
});
process.on('SIGTERM', async () => {
    console.log('🔄 Shutting down server...');
    await server.stop();
    process.exit(0);
});
