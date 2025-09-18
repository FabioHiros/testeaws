# Multi-stage Dockerfile for ESP32 Sensor Data Server

# ----------------------------------------------------------------
# Build stage - Install dependencies and compile TypeScript
# ----------------------------------------------------------------
FROM node:18-alpine AS builder

WORKDIR /app

# Copy package files first for better Docker layer caching
COPY package*.json ./

# Install all dependencies (including devDependencies for TypeScript compilation)
RUN npm ci

# Copy the rest of the source code
COPY . .

# Compile TypeScript to JavaScript using the build script
RUN npm run build


# ----------------------------------------------------------------
# Production stage - Minimal runtime image
# ----------------------------------------------------------------
FROM node:18-alpine AS production

# Add metadata
LABEL maintainer="ESP32 Sensor Project"
LABEL description="MQTT to MongoDB sensor data server"
LABEL version="1.0.0"

# Create app directory and non-root user for security
WORKDIR /app
RUN addgroup -g 1001 -S nodejs && \
    adduser -S nodejs -u 1001

# Copy package files
COPY package*.json ./

# Install only production dependencies
RUN npm ci --omit=dev && \
    npm cache clean --force

# Copy compiled JavaScript from the builder stage
# This is the only code we need to copy
COPY --from=builder /app/dist ./dist

# Change ownership to nodejs user
RUN chown -R nodejs:nodejs /app

# Switch to non-root user
USER nodejs

# Expose port
EXPOSE 3000

# Health check
HEALTHCHECK --interval=30s --timeout=3s --start-period=10s --retries=3 \
  CMD node -e "require('http').get('http://localhost:3000/health', (res) => { process.exit(res.statusCode === 200 ? 0 : 1) }).on('error', () => process.exit(1))"

# Start the application from the correct compiled file
CMD ["node", "dist/server.js"]