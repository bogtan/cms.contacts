<?php

/**
 * Implements hook_admin();
 */
function cms_contacts_admin() {
    $form = array();
    $form[LDAP_ADDRESS] = array(
        '#type' => 'textfield',
        '#title' => t('LDAP Server address'),
        '#default_value' => variable_get(LDAP_ADDRESS, ''),
        '#size' => 60,
        '#maxlength' => 255,
        '#description' => t(""),
        '#required' => TRUE,
    );

    $form[LDAP_PORT] = array(
        '#type' => 'textfield',
        '#title' => t('LDAP Server port'),
        '#default_value' => variable_get(LDAP_PORT, '389'),
        '#size' => 60,
        '#maxlength' => 255,
        '#description' => t(""),
        '#required' => TRUE,
    );

    $form[LDAP_BIND_RDN] = array(
        '#type' => 'textfield',
        '#title' => t('Bind RDN'),
        '#default_value' => variable_get(LDAP_BIND_RDN, ''),
        '#size' => 60,
        '#maxlength' => 255,
        '#description' => t('Bind RDN (Relativ distinguished name)'),
        '#required' => TRUE,
    );

    $form[LDAP_PASSWORD] = array(
        '#type' => 'password',
        '#title' => t('LDAP Server password'),
        '#size' => 25,
        '#description' => t(''),
    );

    $form[LDAP_BASE_DN] = array(
        '#type' => 'textfield',
        '#title' => t('Base DN'),
        '#default_value' => variable_get(LDAP_BASE_DN, ''),
        '#size' => 60,
        '#maxlength' => 255,
        '#description' => t('Base DN (Distinguished name)'),
        '#required' => TRUE,
    );

    $form[LDAP_PEOPLE_DN] = array(
        '#type' => 'textfield',
        '#title' => t('People DN'),
        '#default_value' => variable_get(LDAP_PEOPLE_DN, ''),
        '#size' => 60,
        '#maxlength' => 255,
        '#description' => t('LDAP DN for contacts group'),
        '#required' => TRUE,
    );

    $form[LDAP_ORGANIZATION_DN] = array(
        '#type' => 'textfield',
        '#title' => t('Organization DN'),
        '#default_value' => variable_get(LDAP_ORGANIZATION_DN, ''),
        '#size' => 60,
        '#maxlength' => 255,
        '#description' => t('LDAP DN for organizations group'),
        '#required' => TRUE,
    );

    $form[LDAP_DEPARTMENT_DN] = array(
        '#type' => 'textfield',
        '#title' => t('Department DN'),
        '#default_value' => variable_get(LDAP_DEPARTMENT_DN, ''),
        '#size' => 60,
        '#maxlength' => 255,
        '#description' => t('LDAP DN for departments group'),
        '#required' => TRUE,
    );

    return system_settings_form($form);
}
