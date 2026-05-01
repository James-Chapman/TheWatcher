export function isEnrollmentPending(agent) {
    return !agent.approved && !agent.rejected;
}
export function canManageAgent(agent) {
    return agent.approved && !agent.rejected;
}
export function maintenanceAction(agent) {
    return agent.maintenance ? 'resume' : 'maintenance';
}
export function maintenanceActionLabel(agent) {
    return agent.maintenance ? 'Resume' : 'Maintenance';
}
