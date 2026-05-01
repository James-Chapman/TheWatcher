export type HealthColor = 'green' | 'yellow' | 'amber' | 'red' | 'grey' | 'blue';
export type ComponentKey = 'cpu' | 'memory' | 'disk' | 'network' | 'temperature' | 'processes' | 'heartbeat';
export interface AgentRecord {
    agent_id: string;
    hostname: string;
    platform: string;
    curve_public_key_z85: string;
    approved: boolean;
    rejected: boolean;
    connected: boolean;
    maintenance: boolean;
    maintenance_reason: string;
    maintenance_until: number;
    collection_interval: number;
    process_limit: number;
    first_seen: number;
    last_seen: number;
    group_ids?: number[];
    thresholds?: AgentThresholds;
}
export interface IndicatorThresholds {
    warning_pct_of_avg: number;
    degraded_pct_of_avg: number;
    critical_pct_of_avg: number;
}
export interface AgentThresholds {
    cpu: IndicatorThresholds;
    memory: IndicatorThresholds;
    disk: IndicatorThresholds;
    network: IndicatorThresholds;
}
export interface GroupRecord {
    group_id: number;
    name: string;
    built_in: boolean;
}
export interface UserRecord {
    user_id: number;
    username: string;
    role: 'admin' | 'operator' | 'viewer';
    built_in: boolean;
    disabled: boolean;
    group_ids: number[];
}
export interface SessionInfo {
    username: string;
    role: 'admin' | 'operator' | 'viewer';
}
export interface AlertRecord {
    alert_id: number;
    agent_id: string;
    indicator: string;
    old_status: HealthColor;
    new_status: HealthColor;
    message: string;
    created_at: number;
    acknowledged_by: string;
    acknowledged_at: number;
    deleted_at: number;
}
export interface CpuMetrics {
    usage_percent: number;
    num_logical_cores: number;
    load_avg_1m: number;
}
export interface MemoryMetrics {
    total_bytes: number;
    used_bytes: number;
    usage_percent: number;
}
export interface DiskMetrics {
    device: string;
    mount_point: string;
    filesystem: string;
    total_bytes: number;
    used_bytes: number;
    usage_percent: number;
}
export interface TemperatureMetrics {
    sensor_name: string;
    sensor_label: string;
    temperature_celsius: number;
}
export interface ProcessInfo {
    pid: number;
    name: string;
    status: string;
    cpu_percent: number;
    memory_rss_bytes: number;
    num_threads: number;
}
export interface NetworkMetrics {
    interface_name: string;
    bytes_sent_per_sec: number;
    bytes_recv_per_sec: number;
    errors_in: number;
    errors_out: number;
    drops_in: number;
    drops_out: number;
    is_up: boolean;
}
export interface SystemMetrics {
    cpu: CpuMetrics;
    memory: MemoryMetrics;
    disks: DiskMetrics[];
    temperatures: TemperatureMetrics[];
    top_processes: ProcessInfo[];
    networks: NetworkMetrics[];
    os_name: string;
    os_version: string;
    hostname: string;
    platform: string;
    uptime_seconds: number;
}
export interface MetricsSnapshot {
    agent_id: string;
    timestamp_ms: number;
    metrics: SystemMetrics;
}
export interface ComponentHealth {
    key: ComponentKey;
    label: string;
    color: HealthColor;
    value: string;
    detail: string;
    percent?: number;
}
export interface DashboardAgent {
    id: string;
    name: string;
    platform: string;
    approved: boolean;
    rejected: boolean;
    connected: boolean;
    maintenance: boolean;
    maintenanceReason: string;
    maintenanceUntil: number;
    collectionInterval: number;
    processLimit: number;
    thresholds: AgentThresholds;
    lastSeen: number;
    uptime: string;
    group: string;
    groupIds: number[];
    status: HealthColor;
    alertColor: HealthColor;
    components: ComponentHealth[];
    metrics?: SystemMetrics;
}
