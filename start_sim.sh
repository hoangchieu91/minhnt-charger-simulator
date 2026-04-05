#!/bin/bash

# Configuration
UART_DIR="/tmp/sim_uarts"

echo "🚀 TRU V1.0 - HIL SIMULATION ORCHESTRATOR"
echo "--------------------------------------------------------"

# 1. Clean up and setup directories
echo "[*] Initializing shared UART directory: $UART_DIR"
sudo mkdir -p $UART_DIR
sudo chmod 777 $UART_DIR
rm -f $UART_DIR/ttyTRU

# 2. Check for Docker
if ! docker compose version &> /dev/null; then
    echo "❌ Error: docker compose not found."
    exit 1
fi

# 3. Pull/Build and Start
echo "[*] Spinning up simulation cluster..."
docker compose up --build -d

echo ""
echo "✅ SIMULATION STARTED SUCCESSFULLY"
echo "--------------------------------------------------------"
echo "🌐 Dashboard UI:  file://$(pwd)/dashboard.html"
echo "📡 Dashboard API: http://localhost:4323/api/state"
echo "⚡ STM32 Renode:   localhost:4321 (Modbus TCP)"
echo "🔌 MQTT Broker:    localhost:1883"
echo "--------------------------------------------------------"
echo "Commands:"
echo " - View logs:    docker-compose logs -f"
echo " - Stop all:     docker-compose down"
echo " - Reset STM32:  docker exec -it tru_stm32_sim renode-monitor 'machine reset; start'"
echo "--------------------------------------------------------"
